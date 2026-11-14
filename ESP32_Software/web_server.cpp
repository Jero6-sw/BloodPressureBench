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

WebServer server(80);

// ---------- GET /api/status : statut temps reel (poll par index.html) ----------
void sendJson() {
  StaticJsonDocument<1024> d;
  d["version"] = FW_VERSION;
  d["abp"] = S.abpOk;
  d["pressure"] = S.pressure;
  d["amp"] = S.amp;
  d["ratio"] = S.ratio;
  d["motor"] = S.motorOn;
  d["valve"] = S.valveOpen;
  d["step"] = S.step;
  d["approval"] = S.waitingApproval;
  d["approvalMsg"] = S.approvalMsg;
  String o;
  serializeJson(d, o);
  server.send(200, "application/json", o);
}

// ---------- POST /api/value : saisies operateur (formulaire du rapport) ----------
void handleValue() {
  StaticJsonDocument<256> d;
  deserializeJson(d, server.arg("plain"));
  const char* k = d["key"] | "";
  float v = d["value"] | 0.0f;
  if      (!strcmp(k, "mano"))    S.pReads[d["idx"] | 0] = v;
  else if (!strcmp(k, "display")) S.displayed[d["idx"] | 0] = v;
  else if (!strcmp(k, "temp"))    S.tempC = v;
  else if (!strcmp(k, "boyle")) {
    S.boyleV = (760.0f + S.pressure) * 10.0f / (v != 0 ? v : 1);       // V = P_abs * dV / dP
    float dPexp = (760.0f + S.pressure) * 10.0f / (S.boyleV - 10.0f);  // etape 2 departage : gain attendu
    S.gainErrPct = (dPexp > 0.1f) ? (v - dPexp) / dPexp * 100.0f : 0;   // > ~1,5 mmHg a 100 mmHg => derive electronique
  }
  else if (!strcmp(k, "operator"))strlcpy(S.operatorName, d["value"] | "", sizeof(S.operatorName));
  else if (!strcmp(k, "sn"))      strlcpy(S.deviceSN, d["value"] | "", sizeof(S.deviceSN));
  else if (!strcmp(k, "fwdev"))   strlcpy(S.fwDevice, d["value"] | "", sizeof(S.fwDevice));
  else if (!strcmp(k, "patient")) strlcpy(S.patientName, d["value"] | "", sizeof(S.patientName));
  server.send(200, "application/json", "{}");
}

// ---------- GET /api/report : JSON de session -> PDF genere par le navigateur ----------
void handleReport() {
  StaticJsonDocument<2048> d;
  d["version"] = FW_VERSION;
  d["pressure"] = S.pressure;
  d["leakSlope"] = S.leakSlope;
  d["leakSlopeArm"] = S.leakSlopeArm;
  d["boyleV"] = S.boyleV;
  JsonArray pr = d.createNestedArray("pReads");
  for (float x : S.pReads) pr.add(x);
  JsonArray dp = d.createNestedArray("displayed");
  for (float x : S.displayed) dp.add(x);
  d["tempC"] = S.tempC;
  d["operator"] = S.operatorName;
  d["deviceSN"] = S.deviceSN;
  d["fwDevice"] = S.fwDevice;
  d["zeroOffset"] = S.zeroOffset;
  d["gainErrPct"] = S.gainErrPct;
  d["patient"] = S.patientName;
  String o;
  serializeJson(d, o);
  server.send(200, "application/json", o);
}

// ---------- Endpoints de procedure ----------
void handleStart() {
  String which = server.arg("procedure");
  if (which == "test") procedureStart(TESTS, N_TESTS);
  else                 procedureStart(CALIB, N_CALIB);
  server.send(200, "application/json", "{}");
}

void handleApprove() {
  S.waitingApproval = false;
  procedureAdvance();
  server.send(200, "application/json", "{}");
}

// ---------- GET /api/patients : liste predefinie (config.h PATIENT_LIST) ----------
void handlePatients() {
  StaticJsonDocument<1024> d;
  JsonArray a = d.createNestedArray("patients");
  char buf[sizeof(PATIENT_LIST)];
  strlcpy(buf, PATIENT_LIST, sizeof(buf));
  for (char* p = strtok(buf, "\n"); p; p = strtok(nullptr, "\n")) a.add(p);
  String o; serializeJson(d, o);
  server.send(200, "application/json", o);
}

// ---------- POST /api/publish : publication MQTT (etalonnage + mesures chiffrees) ----------
void handlePublish() {
  bool c = mqttPublishCalib();
  bool m = mqttPublishMeasures();
  String r = "{\"calib\":" + String(c ? "true" : "false") + ",\"mesures\":" + String(m ? "true" : "false") + "}";
  server.send(200, "application/json", r);
}

// ---------- POST /api/wifi : configuration reseau -> NVS, puis redemarrage ----------
void handleWifiConfig() {
  StaticJsonDocument<384> d;
  deserializeJson(d, server.arg("plain"));
  provSave(d["ssid"] | "", d["pass"] | "", d["ip"] | "", d["gw"] | "", d["mask"] | "255.255.255.0");
  server.send(200, "application/json", "{\"saved\":true,\"reboot\":true}");
  delay(500);
  ESP.restart();
}

// ---------- GET /api/config : configuration NVS en cours (sans mot de passe) ----------
void handleGetConfig() {
  StaticJsonDocument<256> d;
  d["configured"] = provHasConfig();
  if (provHasConfig()) {
    ProvCfg c = provLoad();
    d["ssid"] = c.ssid; d["ip"] = c.ip; d["gw"] = c.gw; d["mask"] = c.mask;
  }
  String o; serializeJson(d, o);
  server.send(200, "application/json", o);
}

// ---------- POST /api/reset : efface la NVS reseau et redemarre en mode AP ----------
void handleResetConfig() {
  provClear();
  server.send(200, "application/json", "{\"cleared\":true,\"reboot\":true}");
  delay(500);
  ESP.restart();
}

void setupServer() {
  mqttSetup();
  server.on("/api/status",  HTTP_GET,  sendJson);
  server.on("/api/report",  HTTP_GET,  handleReport);
  server.on("/api/start",   HTTP_POST, handleStart);
  server.on("/api/approve", HTTP_POST, handleApprove);
  server.on("/api/value",   HTTP_POST, handleValue);
  server.on("/api/patients", HTTP_GET,  handlePatients);
  server.on("/api/publish",  HTTP_POST, handlePublish);
  server.on("/api/wifi",    HTTP_POST, handleWifiConfig);    // NVS : reseau + IP fixe
  server.on("/api/config",  HTTP_GET,  handleGetConfig);     // NVS : lecture (sans passe)
  server.on("/api/reset",   HTTP_POST, handleResetConfig);   // NVS : effacement -> AP
  server.serveStatic("/", LittleFS, "/www/");   // page web + jsPDF stockes en flash
  server.begin();
}
