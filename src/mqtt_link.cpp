

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

// Anti-饱和 : la poignee de main TLS est bloquante, on ne retente pas
// en boucle serrée (sinon le serveur web parait mort).
static uint32_t BPB_lastConnMs = 0;
static const uint32_t CONN_RETRY_MS = 5000;   // boucle : toutes les 5 s

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
  // CA epingle. NB : selon la version du core ESP32, setCACert() retourne void
  // (core 3.x) ou bool (core 2.x) -> on valide le PEM par son contenu.
  if (BPB_caPem.indexOf("BEGIN CERTIFICATE") < 0) {
    Serial.println("[MQTT] ERREUR : CA PEM absent ou invalide (coller le bloc BEGIN/END CERTIFICATE complet de ca.crt)");
    return;
  }
  BPB_tlsClient.setCACert(BPB_caPem.c_str());
  BPB_mqtt.setServer(BPB_mqttHost.c_str(), MQTT_PORT);
  mqttActive = true;                          // STA gere par provision (setup du .ino)
  Serial.printf("[MQTT] Actif : %s:%d (CA %s, cle AES OK)\n",
                BPB_mqttHost.c_str(), MQTT_PORT, BPB_caPem.length() ? "epingle" : "ABSENT");
}

bool mqttEnsureConnected() {
  if (!mqttActive || WiFi.status() != WL_CONNECTED) return false;
  if (BPB_mqtt.connected()) return true;
  if (BPB_lastConnMs != 0 && millis() - BPB_lastConnMs < 2000) return false;  // pas de spam TLS
  BPB_lastConnMs = millis();
  bool ok = BPB_mqtt.connect("banc-tensiometre");
  if (ok) { Serial.println("[MQTT] Connexion OK"); return true; }
  // state PubSubClient : -4 = timeout TCP/TLS, -2 = CONNECT refuse (auth),
  // -1 = deconnecte, 1..5 = codes CONNACK broker (1 = protocole, 4/5 = auth).
  Serial.printf("[MQTT] Echec, state=%d : -4=TCP/TLS (listener 8883 ? firewall ? SAN=IP broker ? CA ?) -2/5=auth Mosquitto\n",
                BPB_mqtt.state());
  return ok;
}

void mqttLoop() {
  if (!mqttActive || WiFi.status() != WL_CONNECTED) return;
  if (!BPB_mqtt.connected()) {
    if (millis() - BPB_lastConnMs >= CONN_RETRY_MS) mqttEnsureConnected();   // cadence 5 s
  } else BPB_mqtt.loop();
}

// Diagnostic lisible pour l'interface (reponse /api/publish)
const char* mqttStatus() {
  if (!mqttActive)                return "MQTT inactif : reseau non configure en NVS, ou cle AES absente/trop courte (<32)";
  if (WiFi.status() != WL_CONNECTED) return "WiFi STA non connecte (voir config NVS / portee du reseau)";
  if (!BPB_mqtt.connected())      return "Non connecte au broker : verifier IP broker, listener TLS 8883 et CA epingle";
  return "Connecte au broker";
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
