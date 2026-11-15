







#include "web_server.h"
#include "config.h"
#include "peripherals.h"
#include "procedure.h"
#include "valves.h"
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
  d["abp"] = BPB_BenchState.abpOk;
  d["pressure"] = BPB_BenchState.pressure;
  d["amp"] = BPB_BenchState.amp;
  d["ratio"] = BPB_BenchState.ratio;
  d["motor"] = BPB_BenchState.motorOn;
  d["valve"] = BPB_BenchState.valveOpen;
  d["step"] = BPB_BenchState.step;
  d["approval"] = BPB_BenchState.waitingApproval;
  d["approvalMsg"] = BPB_BenchState.approvalMsg;
  JsonArray vs = d.createNestedArray("valves");
  for (int v = 0; v < 7; v++) vs.add(valveIsOpen(v));    // etat OUVERT des 7 electrovannes
  d["cfg"] = valvesCurrentName();                        // configuration pneumatique en cours
  d["chipTemp"] = BPB_BenchState.chipTempC;              // temperature du chip (degC)
  d["vcc"] = BPB_BenchState.vccmV;                       // tension alim (mV, pont diviseur)
  d["uptime"] = (uint32_t)(millis() / 1000);
  d["rssi"] = WiFi.RSSI();
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
  if      (!strcmp(k, "mano"))    BPB_BenchState.pReads[d["idx"] | 0] = v;
  else if (!strcmp(k, "display")) BPB_BenchState.displayed[d["idx"] | 0] = v;
  else if (!strcmp(k, "temp"))    BPB_BenchState.tempC = v;
  else if (!strcmp(k, "boyle")) {
    BPB_BenchState.boyleV = (760.0f + BPB_BenchState.pressure) * 10.0f / (v != 0 ? v : 1);       // V = P_abs * dV / dP
    float dPexp = (760.0f + BPB_BenchState.pressure) * 10.0f / (BPB_BenchState.boyleV - 10.0f);  // etape 2 departage : gain attendu
    BPB_BenchState.gainErrPct = (dPexp > 0.1f) ? (v - dPexp) / dPexp * 100.0f : 0;   // > ~1,5 mmHg a 100 mmHg => derive electronique
  }
  else if (!strcmp(k, "operator"))strlcpy(BPB_BenchState.operatorName, d["value"] | "", sizeof(BPB_BenchState.operatorName));
  else if (!strcmp(k, "sn"))      strlcpy(BPB_BenchState.deviceSN, d["value"] | "", sizeof(BPB_BenchState.deviceSN));
  else if (!strcmp(k, "fwdev"))   strlcpy(BPB_BenchState.fwDevice, d["value"] | "", sizeof(BPB_BenchState.fwDevice));
  else if (!strcmp(k, "patient")) strlcpy(BPB_BenchState.patientName, d["value"] | "", sizeof(BPB_BenchState.patientName));
  BPB_WebServer.send(200, "application/json", "{}");
}

// ---------- GET /api/report : JSON de session -> PDF genere par le navigateur ----------
void handleReport() {
  StaticJsonDocument<2048> d;
  d["version"] = FW_VERSION;
  d["pressure"] = BPB_BenchState.pressure;
  d["leakSlope"] = BPB_BenchState.leakSlope;
  d["leakSlopeArm"] = BPB_BenchState.leakSlopeArm;
  d["boyleV"] = BPB_BenchState.boyleV;
  JsonArray pr = d.createNestedArray("pReads");
  for (float x : BPB_BenchState.pReads) pr.add(x);
  JsonArray dp = d.createNestedArray("displayed");
  for (float x : BPB_BenchState.displayed) dp.add(x);
  d["tempC"] = BPB_BenchState.tempC;
  d["operator"] = BPB_BenchState.operatorName;
  d["deviceSN"] = BPB_BenchState.deviceSN;
  d["fwDevice"] = BPB_BenchState.fwDevice;
  d["zeroOffset"] = BPB_BenchState.zeroOffset;
  d["gainErrPct"] = BPB_BenchState.gainErrPct;
  d["patient"] = BPB_BenchState.patientName;
  d["seatS0"] = BPB_BenchState.seatS0;          // pente bench zero (A0)
  d["seatArm"] = BPB_BenchState.seatArm;        // pente bras (B)
  d["armLeak"] = BPB_BenchState.armLeak;        // fuite bras deduite
  d["seatBranch"] = BPB_BenchState.seatBranch;  // pente branche clapets (A1)
  String o;
  serializeJson(d, o);
  BPB_WebServer.send(200, "application/json", o);
}

// ---------- POST /api/actuator : pilotage manuel (test banc) ----------
// {key:"motor"|"purge"|"cfg", value:0/1|0..7} ou {key:"v0".."v6", value:0/1}
void handleActuator() {
  StaticJsonDocument<256> d;
  deserializeJson(d, BPB_WebServer.arg("plain"));
  const char* k = d["key"] | "";
  int v = d["value"] | 0;
  if      (!strcmp(k, "motor")) motorRun(v != 0);
  else if (!strcmp(k, "purge")) valveSet(v != 0);
  else if (!strcmp(k, "cfg"))   valvesApply((BenchConfig)constrain(v, 0, N_CONFIGS - 1));
  else if (!strcmp(k, "phaseref")) motorSetPhaseRef();   // came posee sur le repere fin d'aspiration
  else if (k[0] == 'v' && k[1] >= '0' && k[1] <= '6' && k[2] == 0) valveDrive(k[1] - '0', v != 0);
  BPB_WebServer.send(200, "application/json", "{}");
}

// ---------- Endpoints de procedure ----------
void handleStart() {
  String which = BPB_WebServer.arg("procedure");
  if      (which == "test")  procedureStart(TESTS, N_TESTS);
  else if (which == "maint") procedureStart(MAINT,  N_MAINT);
  else                       procedureStart(CALIB,  N_CALIB);
  BPB_WebServer.send(200, "application/json", "{}");
}

void handleApprove() {
  BPB_BenchState.waitingApproval = false;
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
  mqttEnsureConnected();                       // tente la connexion au broker si besoin
  bool okCalib   = mqttPublishCalib();
  bool okMesures = mqttPublishMeasures();
  String r = "{\"calib\":" + String(okCalib ? "true" : "false") +
             ",\"mesures\":" + String(okMesures ? "true" : "false") +
             ",\"detail\":\"" + mqttStatus() + "\"}";
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
  ProvCfg BPB_c = provLoad();
  if (provHasConfig()) {
    d["ssid"] = BPB_c.ssid; d["ip"] = BPB_c.ip; d["gw"] = BPB_c.gw; d["mask"] = BPB_c.mask;
  }
  d["mqttHost"] = BPB_c.mqttHost;                 // broker MQTT (NVS)
  d["caSet"]   = BPB_c.caPem.length()  > 0;       // CA TLS present en NVS (non renvoye)
  d["aesSet"]  = BPB_c.aesKey.length() > 0;       // cle AES presente en NVS (non renvoyee)
  String o; serializeJson(d, o);
  BPB_WebServer.send(200, "application/json", o);
}

// ---------- POST /api/mqtt : broker + CA TLS + cle AES -> NVS, puis redemarrage ----------
// Champs laisses vides = valeurs NVS conservees (pas de re-saisie du PEM/cle).
void handleMqttConfig() {
  StaticJsonDocument<4096> d;
  deserializeJson(d, BPB_WebServer.arg("plain"));
  ProvCfg BPB_cur = provLoad();
  String host = d["host"] | ""; if (!host.length()) host = BPB_cur.mqttHost;
  String ca   = d["ca"]   | ""; if (!ca.length())   ca   = BPB_cur.caPem;
  String aes  = d["aes"]  | ""; if (!aes.length())  aes  = BPB_cur.aesKey;
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
  BPB_WebServer.on("/api/actuator", HTTP_POST, handleActuator);    // test manuel : moteur, purge, vannes
  // Racine explicite : serveStatic ne sert par defaut que "index.htm" a la
  // racine (et notre fichier s'appelle index.html) -> handler dedie, avec
  // diagnostic clair si la partition LittleFS n'a pas ete flashee.
  BPB_WebServer.on("/", HTTP_GET, []() {
    // Tolerant : accepte /www/index.html (data/www/) ou /index.html (data/ a plat)
    const char* candidates[] = { "/www/index.html", "/index.html" };
    for (const char* path : candidates) {
      if (LittleFS.exists(path)) {
        File f = LittleFS.open(path, "r");
        BPB_WebServer.sendHeader("Cache-Control", "no-store");
        BPB_WebServer.streamFile(f, "text/html");
        f.close();
        return;
      }
    }
    BPB_WebServer.send(500, "text/plain",
      "Interface web absente : flasher src/www/ dans la partition LittleFS "
      "(Arduino IDE : outil 'ESP32 Sketch Data Upload' ; PlatformIO : pio run -t uploadfs)");
  });
  BPB_WebServer.serveStatic("/www/", LittleFS, "/www/");   // assets : report.js, jsPDF
  BPB_WebServer.begin();
}
