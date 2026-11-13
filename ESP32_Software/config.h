#ifndef CONFIG_H
#define CONFIG_H

// ---------- Identification ----------
#define FW_VERSION   "1.1.0-banc"

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
#define PIN_VALVE    7              // vanne de purge (V6)

// ---------- Electrovannes 2/2 (variante automatisee, voir docs/automatisation-electrovannes.md) ----------
// Vannes DIRECT-ACTIONNEES des 0 bar obligatoire (circuit a 0,04 MPa max).
// Retenues : SMC VT307V-6G1-01 (12 V DC, ~10 EUR) ou 2V025-06 en solution budget.
// EXCLUES : 2V130-15 / 2W025-06 (diaphragme pilote, mini 0,02-0,05 MPa) et 3V210-08 (0,15 MPa).
// Pilotage GPIO via ULN2803 (7 voies Darlington, diodes de roue libre internes).
// NO = normalement ouverte (energisee = fermee) ; NC = normalement fermee.
#define PIN_V0       15   // V0 (S1 serie, NO)      : fermee = confinement (S1 45 deg)
#define PIN_V1       16   // V1 (PULSATION, NO)     : ouverte = seringue <-> M (mesure)
#define PIN_V2       17   // V2 (POMPE, NC)         : ouverte = seringue <-> branche clapets
#define PIN_V3       18   // V3 (S2 manometre, NC)  : ouverte = lectures croisees
#define PIN_V4       19   // V4 (S3 isolement bras, NC) : ouverte = bras <-> banc
#define PIN_V5       21   // V5 (S4 isolement clapets, NO) : ouverte = ligne CV2 <-> M (pompe)
#define PIN_V6       7    // V6 (purge atmo, NC)    : ouverte = degonflage

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
