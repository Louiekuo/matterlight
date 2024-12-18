// Mired 轉換為 Kelvin，並進一步轉換為 HSV 的函數
void mireds_to_hsv(uint16_t mireds, uint16_t &h, uint8_t &s, uint8_t &v) {
    uint32_t kelvin = 1000000 / mireds; // Mireds 轉換為 Kelvin
    float temperature = kelvin / 100.0;

    // 暫存 RGB 值
    uint8_t r, g, b;

    // 計算 R 值
    if (temperature <= 66) {
        r = 255;
    } else {
        r = temperature - 60;
        r = 329.698727446 * pow(r, -0.1332047592);
        r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    }

    // 計算 G 值
    if (temperature <= 66) {
        g = temperature;
        g = 99.4708025861 * log(g) - 161.1195681661;
        g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    } else {
        g = temperature - 60;
        g = 288.1221695283 * pow(g, -0.0755148492);
        g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    }

    // 計算 B 值
    if (temperature >= 66) {
        b = 255;
    } else {
        if (temperature <= 19) {
            b = 0;
        } else {
            b = temperature - 10;
            b = 138.5177312231 * log(b) - 305.0447927307;
            b = (b < 0) ? 0 : (b > 255) ? 255 : b;
        }
    }

    // 將 RGB 轉換為 HSV
    float rf = r / 255.0;
    float gf = g / 255.0;
    float bf = b / 255.0;

    float max = fmax(fmax(rf, gf), bf);
    float min = fmin(fmin(rf, gf), bf);
    float delta = max - min;

    // 計算 H 值
    float h_float;
    if (delta == 0) {
        h_float = 0;
    } else if (max == rf) {
        h_float = 60 * fmod(((gf - bf) / delta), 6);
    } else if (max == gf) {
        h_float = 60 * (((bf - rf) / delta) + 2);
    } else {
        h_float = 60 * (((rf - gf) / delta) + 4);
    }
    if (h_float < 0) h_float += 360;
    h = static_cast<uint16_t>(h_float);

    // 計算 S 值
    float s_float = (max == 0) ? 0 : (delta / max);
    s = static_cast<uint8_t>(s_float * 255);

    // 計算 V 值
    float v_float = max;
    v = static_cast<uint8_t>(v_float * 255);
}