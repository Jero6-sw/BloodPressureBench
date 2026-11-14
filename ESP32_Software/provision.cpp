

#include "provision.h"

#include <WiFi.h>
#include <Preferences.h>

static Preferences BPB_nvs;   // prefixe BPB_ : eviter toute confusion avec une variable generique

bool provHasConfig() {
  BPB_nvs.begin("banc", true);
  bool ok = BPB_nvs.isKey("ssid");
  BPB_nvs.end();
  return ok;
}

void provSave(const char* ssid, const char* pass,
              const char* ip, const char* gw, const char* mask) {
  BPB_nvs.begin("banc", false);
  BPB_nvs.putString("ssid", ssid);
  BPB_nvs.putString("pass", pass);
  BPB_nvs.putString("ip",   ip);
  BPB_nvs.putString("gw",   gw);
  BPB_nvs.putString("mask", mask);
  BPB_nvs.end();
}

// Config MQTT : IP du broker, CA TLS (PEM) et cle AES-256 (chiffrement des mesures)
void provSaveMqtt(const char* host, const char* ca, const char* aes) {
  BPB_nvs.begin("banc", false);
  BPB_nvs.putString("mhost", host);
  BPB_nvs.putString("ca",    ca);
  BPB_nvs.putString("aes",   aes);
  BPB_nvs.end();
}

ProvCfg provLoad() {
  ProvCfg BPB_c;
  BPB_nvs.begin("banc", true);
  BPB_c.ssid = BPB_nvs.getString("ssid", "");
  BPB_c.pass = BPB_nvs.getString("pass", "");
  BPB_c.ip   = BPB_nvs.getString("ip",   "");
  BPB_c.gw   = BPB_nvs.getString("gw",   "");
  BPB_c.mask = BPB_nvs.getString("mask", "255.255.255.0");
  BPB_c.mqttHost = BPB_nvs.getString("mhost", "");
  BPB_c.caPem    = BPB_nvs.getString("ca",    "");
  BPB_c.aesKey   = BPB_nvs.getString("aes",   "");
  BPB_nvs.end();
  return c;
}

void provClear() {
  BPB_nvs.begin("banc", false);
  BPB_nvs.clear();
  BPB_nvs.end();
}

void provApplySta() {
  ProvCfg BPB_c = provLoad();
  if (BPB_c.ssid.length() == 0) return;
  if (BPB_c.ip.length() > 0) {                       // IP fixe optionnelle
    IPAddress ip, gw, mask;
    ip.fromString(BPB_c.ip);
    gw.fromString(BPB_c.gw);
    mask.fromString(BPB_c.mask);
    WiFi.config(ip, gw, mask);
  }
  WiFi.begin(BPB_c.ssid.c_str(), BPB_c.pass.c_str());     // reconnexion automatique
}

String provSsid() {
  ProvCfg BPB_c = provLoad();
  return BPB_c.ssid;
}
