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

static WiFiClientSecure tlsClient;
static PubSubClient     mqtt(tlsClient);
static bool             mqttActive = false;

// ---------- Chiffrement AES-256-GCM du payload mesures ----------
// Sortie base64( IV(12) || chiffre || TAG(16) ). Cle : MQTT_AES_KEY (NVS en prod).
static String encryptAesGcm(const char* plain, size_t len) {
  unsigned char iv[12], tag[16], out[256];
  for (int i = 0; i < 12; i++) iv[i] = (unsigned char)esp_random();
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, (const unsigned char*)MQTT_AES_KEY, 256);
  mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, len, iv, 12, nullptr, 0,
                            (const unsigned char*)plain, out, 16, tag);
  mbedtls_gcm_free(&gcm);
  String b64 = base64::encode(iv, 12) + ":" + base64::encode(out, len) + ":" + base64::encode(tag, 16);
  return b64;
}

// ---------- Connexion ----------
void mqttSetup() {
  if (!provHasConfig()) return;               // pas de reseau NVS => MQTT inactif
  mqttActive = true;                          // STA gere par provision (setup du .ino)
  if (strlen(MQTT_CA_CERT) > 0) tlsClient.setCACert(MQTT_CA_CERT);  // CA epingle
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

void mqttLoop() {
  if (!mqttActive || WiFi.status() != WL_CONNECTED) return;
  if (!mqtt.connected()) mqtt.connect("banc-tensiometre");
  else mqtt.loop();
}

// ---------- Publication des mesures d'etalonnage ----------
bool mqttPublishCalib() {
  if (!mqttActive || !mqtt.connected()) return false;
  StaticJsonDocument<768> d;
  d["zeroOffset"]  = S.zeroOffset;            // etape 1 : zero ABP air libre
  d["gainErrPct"]  = S.gainErrPct;            // etape 2 : gain Boyle
  d["leakSlope"]   = S.leakSlope;             // etape 3 : croisement -> pReads
  d["leakSlopeArm"]= S.leakSlopeArm;
  d["boyleV"]      = S.boyleV;
  JsonArray pr = d.createNestedArray("pReads");
  for (float x : S.pReads) pr.add(x);
  d["tempC"] = S.tempC;
  d["operator"] = S.operatorName;
  d["version"] = FW_VERSION;
  String o; serializeJson(d, o);
  return mqtt.publish(MQTT_TOPIC "/calib", o.c_str());
}

// ---------- Publication des mesures de tension (chiffrees) ----------
bool mqttPublishMeasures() {
  if (!mqttActive || !mqtt.connected()) return false;
  StaticJsonDocument<512> d;
  d["patient"]  = S.patientName;              // liste predefinie, choisi en debut de test
  d["operator"] = S.operatorName;
  d["deviceSN"] = S.deviceSN;
  d["tempC"]    = S.tempC;
  JsonArray ms = d.createNestedArray("displayed");
  for (float x : S.displayed) ms.add(x);
  String o; serializeJson(d, o);
  String enc = encryptAesGcm(o.c_str(), o.length());   // AES-256-GCM bout en bout
  return mqtt.publish(MQTT_TOPIC "/mesures", enc.c_str());
}
