

#include "mqtt_link.h"
#include "config.h"
#include "peripherals.h"
#include "provision.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <mbedtls/gcm.h>
#include <base64.h>

static WiFiClientSecure BPB_tlsClient;
static PubSubClient     BPB_mqtt(BPB_tlsClient);
static bool             mqttActive = false;

// Config effective : NVS si renseignee, sinon repli config.h
static String BPB_mqttHost, BPB_caPem, BPB_aesKey;

// ---------- Chiffrement AES-256-GCM du payload mesures ----------
// Sortie base64( IV(12) || chiffre || TAG(16) ). Cle : NVS (provision).
static String encryptAesGcm(const char* plain, size_t len) {
  unsigned char iv[12], tag[16], out[256];
  for (int i = 0; i < 12; i++) iv[i] = (unsigned char)esp_random();
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, (const unsigned char*)BPB_aesKey.c_str(), 256);
  mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, len, iv, 12, nullptr, 0,
                            (const unsigned char*)plain, out, 16, tag);
  mbedtls_gcm_free(&gcm);
  String b64 = base64::encode(iv, 12) + ":" + base64::encode(out, len) + ":" + base64::encode(tag, 16);
  return b64;
}

// ---------- Connexion ----------
void mqttSetup() {
  if (!provHasConfig()) return;               // pas de reseau NVS => MQTT inactif
  ProvCfg BPB_c = provLoad();
  BPB_mqttHost = BPB_c.mqttHost.length() ? BPB_c.mqttHost : MQTT_HOST;
  BPB_caPem    = BPB_c.caPem.length()    ? BPB_c.caPem    : MQTT_CA_CERT;
  BPB_aesKey   = BPB_c.aesKey.length()   ? BPB_c.aesKey   : MQTT_AES_KEY;
  if (BPB_aesKey.length() < 32) {                  // cle AES obligatoire (32 octets)
    Serial.println("[MQTT] Cle AES absente ou trop courte : MQTT inactif");
    return;
  }
  if (BPB_caPem.length() > 0) BPB_tlsClient.setCACert(BPB_caPem.c_str());  // CA epingle
  BPB_mqtt.setServer(BPB_mqttHost.c_str(), MQTT_PORT);
  mqttActive = true;                          // STA gere par provision (setup du .ino)
}

void mqttLoop() {
  if (!mqttActive || WiFi.status() != WL_CONNECTED) return;
  if (!BPB_mqtt.connected()) BPB_mqtt.connect("banc-tensiometre");
  else BPB_mqtt.loop();
}

// ---------- Publication des mesures d'etalonnage ----------
bool mqttPublishCalib() {
  if (!mqttActive || !BPB_mqtt.connected()) return false;
  StaticJsonDocument<768> d;
  d["zeroOffset"]  = BPB_BenchState.zeroOffset;            // etape 1 : zero ABP air libre
  d["gainErrPct"]  = BPB_BenchState.gainErrPct;            // etape 2 : gain Boyle
  d["leakSlope"]   = BPB_BenchState.leakSlope;             // etape 3 : croisement -> pReads
  d["leakSlopeArm"]= BPB_BenchState.leakSlopeArm;
  d["boyleV"]      = BPB_BenchState.boyleV;
  JsonArray pr = d.createNestedArray("pReads");
  for (float x : BPB_BenchState.pReads) pr.add(x);
  d["tempC"] = BPB_BenchState.tempC;
  d["operator"] = BPB_BenchState.operatorName;
  d["version"] = FW_VERSION;
  String o; serializeJson(d, o);
  return BPB_mqtt.publish(MQTT_TOPIC "/calib", o.c_str());
}

// ---------- Publication des mesures de tension (chiffrees) ----------
bool mqttPublishMeasures() {
  if (!mqttActive || !BPB_mqtt.connected()) return false;
  StaticJsonDocument<512> d;
  d["patient"]  = BPB_BenchState.patientName;              // liste predefinie, choisi en debut de test
  d["operator"] = BPB_BenchState.operatorName;
  d["deviceSN"] = BPB_BenchState.deviceSN;
  d["tempC"]    = BPB_BenchState.tempC;
  JsonArray ms = d.createNestedArray("displayed");
  for (float x : BPB_BenchState.displayed) ms.add(x);
  String o; serializeJson(d, o);
  String enc = encryptAesGcm(o.c_str(), o.length());   // AES-256-GCM bout en bout
  return BPB_mqtt.publish(MQTT_TOPIC "/mesures", enc.c_str());
}
