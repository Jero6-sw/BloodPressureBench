#ifndef CONFIG_H
#define CONFIG_H

// ---------- Identification ----------
#define FW_VERSION   "1.0.0-banc"

// ---------- Capteur ABP (Honeywell ABPDRRV040MG2A3, 0-40 kPa, I2C) ----------
#define ABP_ADDR     0x28
#define ABP_FULLSCALE_KPA  40.0f     // plage 0-40 kPa (gauge)
#define KPA2MMHG     7.50062f

// ---------- Brochage ESP32-S3 ----------
#define PIN_SDA      8
#define PIN_SCL      9
#define PIN_STEP     4
#define PIN_DIR      5
#define PIN_EN       6              // actif bas (TMC2209)
#define PIN_VALVE    7              // TIP120 -> electrovanne 12 V

// ---------- Consignes pneumatiques ----------
#define P_TARGET_MMHG      100.0f   // P_int de session
#define P_TOLERANCE_MMHG   2.0f
#define P_LEAKTEST_MMHG    150.0f
#define LEAK_SLOPE_MAX     1.0f     // mmHg/min
#define STEP_PERIOD_US     313      // 60 tr/min (3200 µsteps/tour -> 1 Hz)
#define ACQ_PERIOD_MS      10       // acquisition 100 Hz

// ---------- Metriques de forme ----------
#define SHAPE_RATIO_ALERT  1.8f     // ratio montee/descente -> mousse EPDM si depasse

// ---------- WiFi (reseau propre au banc) ----------
#define AP_SSID      "Banc-Tensiometre"
#define AP_PASSWORD  "Banc10ans!"

#endif
