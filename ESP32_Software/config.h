
#ifndef CONFIG_H
#define CONFIG_H

// ---------- Identification ----------
#define FW_VERSION   "1.5.1-banc"

// ---------- Capteur ABP (Honeywell ABPDRRV040MG2A3, 0-40 kPa, I2C) ----------
#define ABP_ADDR     0x28
#define ABP_FULLSCALE_KPA  40.0f     // plage 0-40 kPa (gauge)
#define KPA2MMHG     7.50062f

// ---------- Brochage ESP32-WROOM-32 (DevKit v1) ----------
// Rappel WROOM-32 : GPIO 6-11 = flash interne (INTERDITS en E/S),
// GPIO 34-39 = entree seule, strapping a eviter en sortie : 0, 2, 12, 15.
#define PIN_SDA      21             // I2C capteur ABP
#define PIN_SCL      22             // I2C capteur ABP
#define PIN_STEP     4
#define PIN_DIR      5
#define PIN_EN       23             // actif bas (TMC2209)
#define PIN_VALVE    33             // vanne de purge (V6)

// ---------- Electrovannes 2/2 (variante automatisee, voir docs/automatisation-electrovannes.md) ----------
// Vannes DIRECT-ACTIONNEES des 0 bar obligatoire (circuit a 0,04 MPa max).
// Retenues : SMC VT307V-6G1-01 (12 V DC, ~10 EUR) ou 2V025-06 en solution budget.
// EXCLUES : 2V130-15 / 2W025-06 (diaphragme pilote, mini 0,02-0,05 MPa) et 3V210-08 (0,15 MPa).
// Pilotage GPIO via ULN2803 (7 voies Darlington, diodes de roue libre internes).
// NO = normalement ouverte (energisee = fermee) ; NC = normalement fermee.
#define PIN_V0       13   // V0 (S1 serie, NO)      : fermee = confinement (S1 45 deg)
#define PIN_V1       14   // V1 (PULSATION, NO)     : ouverte = seringue <-> M (mesure)
#define PIN_V2       16   // V2 (POMPE, NC)         : ouverte = seringue <-> branche clapets
#define PIN_V3       17   // V3 (S2 manometre, NC)  : ouverte = lectures croisees
#define PIN_V4       18   // V4 (S3 isolement bras, NC) : ouverte = bras <-> banc
#define PIN_V5       32   // V5 (S4 isolement clapets, NO) : ouverte = ligne CV2 <-> M (pompe)
#define PIN_V6       33   // V6 (purge atmo, NC)    : ouverte = degonflage

// ---------- Consignes pneumatiques ----------
#define P_TARGET_MMHG      100.0f   // P_int de session
#define P_TOLERANCE_MMHG   2.0f
#define P_LEAKTEST_MMHG    150.0f
#define LEAK_SLOPE_MAX     1.0f     // mmHg/min
#define STEP_PERIOD_US     313      // 60 tr/min (3200 µsteps/tour -> 1 Hz)
#define ACQ_PERIOD_MS      10       // acquisition 100 Hz

// ---------- Metriques de forme ----------
#define SHAPE_RATIO_ALERT  1.8f     // ratio montee/descente -> mousse EPDM si depasse

// ---------- WiFi (AP de configuration, 1er demarrage uniquement) ----------
// Des qu'un reseau est configure en NVS, l'AP est coupe (mode WIFI_STA).
#define AP_SSID      "Banc-Tensiometre"
#define AP_PASSWORD  "Banc10ans!"

// ---------- MQTT : stockage des mesures + chiffrement ----------
// Tout est configurable via l'interface web et stocke en NVS (provision.h) :
// IP du broker, CA TLS (PEM) et cle AES-256. Les valeurs ci-dessous ne sont
// que des VALEURS DE REPLI si la NVS est vide (MQTT_HOST/MQTT_CA_CERT/MQTT_AES_KEY).
// Pas de reseau configure en NVS => MQTT inactif.
#define MQTT_PORT      8883              // TLS obligatoire
#define MQTT_TOPIC     "banc/tensiometre"
#define MQTT_HOST      "192.168.1.10"    // repli : broker (Mosquitto sur Raspberry/NAS)
#define MQTT_CA_CERT   ""                // repli : PEM du CA du broker
#define MQTT_AES_KEY   "CHANGEZ_MOI_cle_32_octets________"   // repli : cle AES-256
// Liste predefinie des patients (un nom par ligne) - demandee en debut de test.
#define PATIENT_LIST   "Patient A\nPatient B\nPatient C\n"

#endif
