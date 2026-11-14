#include "provision.h"

#include <WiFi.h>
#include <Preferences.h>

static Preferences nvs;

bool provHasConfig() {
  nvs.begin("banc", true);
  bool ok = nvs.isKey("ssid");
  nvs.end();
  return ok;
}

void provSave(const char* ssid, const char* pass,
              const char* ip, const char* gw, const char* mask) {
  nvs.begin("banc", false);
  nvs.putString("ssid", ssid);
  nvs.putString("pass", pass);
  nvs.putString("ip",   ip);
  nvs.putString("gw",   gw);
  nvs.putString("mask", mask);
  nvs.end();
}

// Config MQTT : IP du broker, CA TLS (PEM) et cle AES-256 (chiffrement des mesures)
void provSaveMqtt(const char* host, const char* ca, const char* aes) {
  nvs.begin("banc", false);
  nvs.putString("mhost", host);
  nvs.putString("ca",    ca);
  nvs.putString("aes",   aes);
  nvs.end();
}

ProvCfg provLoad() {
  ProvCfg c;
  nvs.begin("banc", true);
  c.ssid = nvs.getString("ssid", "");
  c.pass = nvs.getString("pass", "");
  c.ip   = nvs.getString("ip",   "");
  c.gw   = nvs.getString("gw",   "");
  c.mask = nvs.getString("mask", "255.255.255.0");
  c.mqttHost = nvs.getString("mhost", "");
  c.caPem    = nvs.getString("ca",    "");
  c.aesKey   = nvs.getString("aes",   "");
  nvs.end();
  return c;
}

void provClear() {
  nvs.begin("banc", false);
  nvs.clear();
  nvs.end();
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
