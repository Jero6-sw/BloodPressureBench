

#include "web_server.h"
#include "config.h"
#include "peripherals.h"
#include "procedure.h"
#include "mqtt_link.h"
#include "provision.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

WebServer BPB_WebServer(80);   // definition ; declaree dans web_server.h

// ---------- GET /api/status : statut temps reel (poll par index.html) ----------
void sendJson() {
  StaticJsonDocument<1024> d;
  d["version"] = FW_VERSION;
  d["abp"] = BPB_S.abpOk;
  d["pressure"] = BPB_S.pressure;
  d["amp"] = BPB_S.amp;
  d["ratio"] = BPB_S.ratio;
  d["motor"] = BPB_S.motorOn;
  d["valve"] = BPB_S.valveOpen;
  d["step"] = BPB_S.step;
  d["approval"] = BPB_S.waitingApproval;
  d["approvalMsg"] = BPB_S.approvalMsg;
  String o;
  serializeJson(d, o);
  BPB_WebServer.send(200, "application/json", o);
}

// ---------- POST /api/value : saisies operateur (formulaire du rapport) ----------
void handleValue() {
  StaticJsonDocument<256> d;
  deserializeJson(d, BPB_WebServer.arg("plain"));
  const char* k = d["key"] | "";
  float v = d["value"] | 0.0f;
  if      (!strcmp(k, "mano"))    BPB_S.pReads[d["idx"] | 0] = v;
  else if (!strcmp(k, "display")) BPB_S.displayed[d["idx"] | 0] = v;
  else if (!strcmp(k, "temp"))    BPB_S.tempC = v;
  else if (!strcmp(k, "boyle")) {
    BPB_S.boyleV = (760.0f + BPB_S.pressure) * 10.0f / (v != 0 ? v : 1);       // V = P_abs * dV / dP
    float dPexp = (760.0f + BPB_S.pressure) * 10.0f / (BPB_S.boyleV - 10.0f);  // etape 2 departage : gain attendu
    BPB_S.gainErrPct = (dPexp > 0.1f) ? (v - dPexp) / dPexp * 100.0f : 0;   // > ~1,5 mmHg a 100 mmHg => derive electronique
  }
  else if (!strcmp(k, "operator"))strlcpy(BPB_S.operatorName, d["value"] | "", sizeof(BPB_S.operatorName));
  else if (!strcmp(k, "sn"))      strlcpy(BPB_S.deviceSN, d["value"] | "", sizeof(BPB_S.deviceSN));
  else if (!strcmp(k, "fwdev"))   strlcpy(BPB_S.fwDevice, d["value"] | "", sizeof(BPB_S.fwDevice));
  else if (!strcmp(k, "patient")) strlcpy(BPB_S.patientName, d["value"] | "", sizeof(BPB_S.patientName));
  BPB_WebServer.send(200, "application/json", "{}");
}

// ---------- GET /api/report : JSON de session -> PDF genere par le navigateur ----------
void handleReport() {
  StaticJsonDocument<2048> d;
  d["version"] = FW_VERSION;
  d["pressure"] = BPB_S.pressure;
  d["leakSlope"] = BPB_S.leakSlope;
  d["leakSlopeArm"] = BPB_S.leakSlopeArm;
  d["boyleV"] = BPB_S.boyleV;
  JsonArray pr = d.createNestedArray("pReads");
  for (float x : BPB_S.pReads) pr.add(x);
  JsonArray dp = d.createNestedArray("displayed");
  for (float x : BPB_S.displayed) dp.add(x);
  d["tempC"] = BPB_S.tempC;
  d["operator"] = BPB_S.operatorName;
  d["deviceSN"] = BPB_S.deviceSN;
  d["fwDevice"] = BPB_S.fwDevice;
  d["zeroOffset"] = BPB_S.zeroOffset;
  d["gainErrPct"] = BPB_S.gainErrPct;
  d["patient"] = BPB_S.patientName;
  String o;
  serializeJson(d, o);
  BPB_WebServer.send(200, "application/json", o);
}

// ---------- Endpoints de procedure ----------
void handleStart() {
  String which = BPB_WebServer.arg("procedure");
  if (which == "test") procedureStart(TESTS, N_TESTS);
  else                 procedureStart(CALIB, N_CALIB);
  BPB_WebServer.send(200, "application/json", "{}");
}

void handleApprove() {
  BPB_S.waitingApproval = false;
  procedureAdvance();
  BPB_WebServer.send(200, "application/json", "{}");
}

// ---------- GET /api/patients : liste predefinie (config.h PATIENT_LIST) ----------
void handlePatients() {
  StaticJsonDocument<1024> d;
  JsonArray a = d.createNestedArray("patients");
  char buf[sizeof(PATIENT_LIST)];
  strlcpy(buf, PATIENT_LIST, sizeof(buf));
  for (char* p = strtok(buf, "\n"); p; p = strtok(nullptr, "\n")) a.add(p);
  String o; serializeJson(d, o);
  BPB_WebServer.send(200, "application/json", o);
}

// ---------- POST /api/publish : publication MQTT (etalonnage + mesures chiffrees) ----------
void handlePublish() {
  bool c = mqttPublishCalib();
  bool m = mqttPublishMeasures();
  String r = "{\"calib\":" + String(c ? "true" : "false") + ",\"mesures\":" + String(m ? "true" : "false") + "}";
  BPB_WebServer.send(200, "application/json", r);
}

// ---------- POST /api/wifi : configuration reseau -> NVS, puis redemarrage ----------
void handleWifiConfig() {
  StaticJsonDocument<384> d;
  deserializeJson(d, BPB_WebServer.arg("plain"));
  provSave(d["ssid"] | "", d["pass"] | "", d["ip"] | "", d["gw"] | "", d["mask"] | "255.255.255.0");
  BPB_WebServer.send(200, "application/json", "{\"saved\":true,\"reboot\":true}");
  delay(500);
  ESP.restart();
}

// ---------- GET /api/config : configuration NVS en cours (sans secrets) ----------
void handleGetConfig() {
  StaticJsonDocument<512> d;
  d["configured"] = provHasConfig();
  ProvCfg c = provLoad();
  if (provHasConfig()) {
    d["ssid"] = c.ssid; d["ip"] = c.ip; d["gw"] = c.gw; d["mask"] = c.mask;
  }
  d["mqttHost"] = c.mqttHost;                 // broker MQTT (NVS)
  d["caSet"]   = c.caPem.length()  > 0;       // CA TLS present en NVS (non renvoye)
  d["aesSet"]  = c.aesKey.length() > 0;       // cle AES presente en NVS (non renvoyee)
  String o; serializeJson(d, o);
  BPB_WebServer.send(200, "application/json", o);
}

// ---------- POST /api/mqtt : broker + CA TLS + cle AES -> NVS, puis redemarrage ----------
// Champs laisses vides = valeurs NVS conservees (pas de re-saisie du PEM/cle).
void handleMqttConfig() {
  StaticJsonDocument<4096> d;
  deserializeJson(d, BPB_WebServer.arg("plain"));
  ProvCfg cur = provLoad();
  String host = d["host"] | ""; if (!host.length()) host = cur.mqttHost;
  String ca   = d["ca"]   | ""; if (!ca.length())   ca   = cur.caPem;
  String aes  = d["aes"]  | ""; if (!aes.length())  aes  = cur.aesKey;
  provSaveMqtt(host.c_str(), ca.c_str(), aes.c_str());
  BPB_WebServer.send(200, "application/json", "{\"saved\":true,\"reboot\":true}");
  delay(500);
  ESP.restart();
}

// ---------- POST /api/reset : efface la NVS reseau et redemarre en mode AP ----------
void handleResetConfig() {
  provClear();
  BPB_WebServer.send(200, "application/json", "{\"cleared\":true,\"reboot\":true}");
  delay(500);
  ESP.restart();
}

void setupServer() {
  mqttSetup();
  BPB_WebServer.on("/api/status",  HTTP_GET,  sendJson);
  BPB_WebServer.on("/api/report",  HTTP_GET,  handleReport);
  BPB_WebServer.on("/api/start",   HTTP_POST, handleStart);
  BPB_WebServer.on("/api/approve", HTTP_POST, handleApprove);
  BPB_WebServer.on("/api/value",   HTTP_POST, handleValue);
  BPB_WebServer.on("/api/patients", HTTP_GET,  handlePatients);
  BPB_WebServer.on("/api/publish",  HTTP_POST, handlePublish);
  BPB_WebServer.on("/api/wifi",    HTTP_POST, handleWifiConfig);    // NVS : reseau + IP fixe
  BPB_WebServer.on("/api/mqtt",    HTTP_POST, handleMqttConfig);    // NVS : broker + CA TLS + cle AES
  BPB_WebServer.on("/api/config",  HTTP_GET,  handleGetConfig);     // NVS : lecture (sans secrets)
  BPB_WebServer.on("/api/reset",   HTTP_POST, handleResetConfig);   // NVS : effacement -> AP
  BPB_WebServer.serveStatic("/", LittleFS, "/www/");   // page web + jsPDF stockes en flash
  BPB_WebServer.begin();
}
