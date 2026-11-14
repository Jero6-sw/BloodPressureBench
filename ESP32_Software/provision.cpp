
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
  ProvCfg c;
  BPB_nvs.begin("banc", true);
  c.ssid = BPB_nvs.getString("ssid", "");
  c.pass = BPB_nvs.getString("pass", "");
  c.ip   = BPB_nvs.getString("ip",   "");
  c.gw   = BPB_nvs.getString("gw",   "");
  c.mask = BPB_nvs.getString("mask", "255.255.255.0");
  c.mqttHost = BPB_nvs.getString("mhost", "");
  c.caPem    = BPB_nvs.getString("ca",    "");
  c.aesKey   = BPB_nvs.getString("aes",   "");
  BPB_nvs.end();
  return c;
}

void provClear() {
  BPB_nvs.begin("banc", false);
  BPB_nvs.clear();
  BPB_nvs.end();
}

void provApplySta() {
  ProvCfg c = provLoad();
  if (c.ssid.length() == 0) return;
  if (c.ip.length() > 0) {                       // IP fixe optionnelle
    IPAddress ip, gw, mask;
    ip.fromString(c.ip);
    gw.fromString(c.gw);
    mask.fromString(c.mask);
    WiFi.config(ip, gw, mask);
  }
  WiFi.begin(c.ssid.c_str(), c.pass.c_str());     // reconnexion automatique
}

String provSsid() {
  ProvCfg c = provLoad();
  return c.ssid;
}
