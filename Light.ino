#include <Adafruit_NeoPixel.h>
#include "Matter.h"
#include <esp_matter.h>
#include <app/server/OnboardingCodesUtil.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include "func.h"
using namespace chip;
using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::endpoint;

#define REMAP_TO_RANGE(value, from, to) ((value * (float)to) / (float)from)

static uint16_t hue = 0;          // 色相 (0-360)，改為 uint16_t
static uint8_t saturation = 0;    // 飽和度 (0-255)，改為 uint8_t
static uint8_t brightness = 255;  // 亮度 (假設默認值)，改為 uint8_t

uint32_t color;
const int LED_PIN = 1;

// Cluster and attribute ID used by Matter light device
const uint32_t CLUSTER_ID = OnOff::Id, CID_LevelCtl = LevelControl::Id, CID_ColorCtl = ColorControl::Id;
const uint32_t ATTRIBUTE_ID = OnOff::Attributes::OnOff::Id, AID_LevelCtl = LevelControl::Attributes::CurrentLevel::Id,
               AID_ColorCtlHue = ColorControl::Attributes::CurrentHue::Id, AID_ColorCtlSat = ColorControl::Attributes::CurrentSaturation::Id, AID_ColorCtlCol = ColorControl::Attributes::ColorTemperatureMireds::Id;

// Endpoint and attribute ref that will be assigned to Matter device
//這個是電燈的endpoint_id
uint16_t light_endpoint_id = 0;

#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// There is possibility to listen for various device events, related for example to setup process
// Leaved as empty for simplicity
static void on_device_event(const ChipDeviceEvent *event, intptr_t arg){};
static esp_err_t on_identification(identification::callback_type_t type, uint16_t endpoint_id,
                                   uint8_t effect_id, uint8_t effect_variant, void *priv_data) {
  return ESP_OK;
}

// Listener on attribute update requests.
// In this example, when update is requested, path (endpoint, cluster and attribute) is checked
// if it matches light attribute. If yes, LED changes state to new one.
static esp_err_t on_attribute_update(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                     uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data) {

  // 處理 On/Off 屬性的更新 (開關)
  if (type == attribute::PRE_UPDATE && endpoint_id == light_endpoint_id && cluster_id == CLUSTER_ID && attribute_id == ATTRIBUTE_ID) {
    bool new_state = val->val.b;  // 讀取新的開關狀態
    if (new_state) {
      // 開燈，設置為白色
      color = pixels.ColorHSV(hue, saturation, brightness);
    } else {
      // 關燈
      color = pixels.ColorHSV(hue, saturation, 0);
    }
  }
  // 處理 LevelControl 屬性 (亮度)
  if (type == attribute::PRE_UPDATE && endpoint_id == light_endpoint_id && cluster_id == CID_LevelCtl && attribute_id == AID_LevelCtl) {
    brightness = REMAP_TO_RANGE(val->val.u8, 254, 255);
    color = pixels.ColorHSV(hue, saturation, brightness);
  }
  // 處理 ColorControl 屬性 (顏色)
  if (type == attribute::PRE_UPDATE && endpoint_id == light_endpoint_id && cluster_id == CID_ColorCtl) {
    // 更新燈光顯示（Hue 和 Saturation 更新時）
    if (attribute_id == AID_ColorCtlHue || attribute_id == AID_ColorCtlSat) {
      if (attribute_id == AID_ColorCtlHue) {
        hue = REMAP_TO_RANGE(val->val.u8, 254, 65536);
      } else if (attribute_id == AID_ColorCtlSat) {
        saturation = REMAP_TO_RANGE(val->val.u8, 254, 255);
      }
      color = pixels.ColorHSV(hue, saturation, brightness);
    } else if (attribute_id == AID_ColorCtlCol) {
      // 處理色溫
      mireds_to_hsv(val->val.u16, hue, saturation, brightness);
      hue = REMAP_TO_RANGE(hue, 360, 65536);
      color = pixels.ColorHSV(hue, saturation, brightness);
    }
  }
  pixels.fill(color, 1, 1);
  delay(1);
  pixels.show();

  return ESP_OK;
}

void setup() {
  Serial.begin(115200);
  // Enable debug logging
  esp_log_level_set("*", ESP_LOG_DEBUG);

  // Setup Matter node
  node::config_t node_config;
  node_t *node = node::create(&node_config, on_attribute_update, on_identification);

  //設定燈在初始時為關閉狀態
  extended_color_light::config_t light_config;
  light_config.on_off.on_off = false;
  light_config.on_off.lighting.start_up_on_off = false;
  light_config.level_control.current_level = nullptr;
  light_config.level_control.on_level = nullptr;
  light_config.color_control.hue_saturation.current_saturation = 0;
  light_config.color_control.hue_saturation.current_hue = 0;
  light_config.color_control.color_mode = (uint8_t)ColorControl::ColorMode::EMBER_ZCL_COLOR_MODE_CURRENT_HUE_AND_CURRENT_SATURATION;
  light_config.color_control.enhanced_color_mode = (uint8_t)ColorControl::ColorMode::EMBER_ZCL_COLOR_MODE_CURRENT_HUE_AND_CURRENT_SATURATION;

  endpoint_t *endpoint = extended_color_light::create(node, &light_config, ENDPOINT_FLAG_NONE, NULL);

  // **啟用 Hue-Saturation Feature**
  cluster_t *color_control_cluster = cluster::get(endpoint, ColorControl::Id);
  cluster::color_control::feature::hue_saturation::config_t hue_saturation_config;
  // hue_saturation_config.current_hue = 0;         // 設定預設色相 (0-254)
  // hue_saturation_config.current_saturation = 0;  // 設定預設飽和度 (0-254)
  cluster::color_control::feature::hue_saturation::add(color_control_cluster, &hue_saturation_config);

  // Save generated endpoint id
  light_endpoint_id = endpoint::get_id(endpoint);

  // Setup DAC (this is good place to also set custom commission data, passcodes etc.)
  esp_matter::set_custom_dac_provider(chip::Credentials::Examples::GetExampleDACProvider());

  // Start Matter device
  esp_matter::start(on_device_event);

  // Print codes needed to setup Matter device
  PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));
  pixels.begin();
  pixels.setBrightness(0);
  pixels.show();
}

void loop() {
}
