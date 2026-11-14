/*
 * Banc de test - derive d'un ensemble tensiometre + brassard
 * Firmware ESP32-WROOM-32 - voir README_FR.md / README_EN.md a la racine du depot.
 *
 * Modules :
 *   config.h        brochage, constantes, identifiants WiFi
 *   peripherals.*   capteur ABP, moteur, vanne, acquisition 100 Hz
 *   procedure.*     moteur de procedure (auto-diagnostic + test)
 *   web_server.*    serveur web / API REST
 *   provision.*     NVS : reseau + MQTT (broker, CA TLS, cle AES)
 *   mqtt_link.*     publication MQTT TLS + chiffrement AES-256-GCM
 *   www/            interface + generation du rapport PDF (jsPDF, cote navigateur)
 */
#include <Wire.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "config.h"
#include "peripherals.h"
#include "procedure.h"
#include "web_server.h"
#include "mqtt_link.h"
#include "provision.h"

static uint32_t staStartMs = 0;
static bool     fallbackAp = false;

// AP coupe des qu'un reseau est configure ; reactive en secours seulement
// si le reseau sauvegarde reste introuvable apres 60 s (reconfiguration possible).
static void manageNetwork() {
  if (!provHasConfig() || fallbackAp) return;
  if (WiFi.status() != WL_CONNECTED && millis() - staStartMs > 60000UL) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    fallbackAp = true;
    Serial.println("[BANC] STA injoignable : AP de configuration reactive (192.168.4.1)");
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);

  // Sorties actionneurs - etat de securite au demarrage
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR,  OUTPUT);
  pinMode(PIN_EN,   OUTPUT);
  pinMode(PIN_VALVE, OUTPUT);
  digitalWrite(PIN_DIR, LOW);
  motorRun(false);
  valveSet(false);

  // Capteur ABP present ?
  S.abpOk = abpPresent();
  Serial.printf("[BANC] v%s - ABP %s\n", FW_VERSION, S.abpOk ? "OK" : "ABSENT");

  // Systeme de fichiers (interface web + jsPDF)
  if (!LittleFS.begin(true)) Serial.println("[BANC] LittleFS: formatage/montage");

  // Reseau : avec config NVS -> STA seul (AP coupe) ; sans config -> AP de configuration
  staStartMs = millis();
  if (provHasConfig()) {
    WiFi.mode(WIFI_STA);                        // AP coupe : le banc rejoint le reseau sauvegarde
    provApplySta();                            // IP fixe + connexion automatique
    Serial.printf("[BANC] STA auto : %s (AP coupe)\n", provSsid().c_str());
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);          // AP : interface web (192.168.4.1)
    Serial.printf("[BANC] Pas de config NVS : AP %s - IP ", AP_SSID);
    Serial.println(WiFi.softAPIP());
  }

  // Tache d'acquisition sur le coeur 0 (moteur + ABP 100 Hz)
  startAcquisitionTask();

  // Serveur web sur le coeur 1
  setupServer();
}

void loop() {
  server.handleClient();
  manageNetwork();   // secours : reactiver l'AP si le STA reste injoignable
  mqttLoop();        // MQTT TLS : keepalive + reconnexion
}
