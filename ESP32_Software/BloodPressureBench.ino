/*
 * Banc de test - derive d'un ensemble tensiometre + brassard
 * Firmware ESP32-S3 - voir README_FR.md / README_EN.md a la racine du depot.
 *
 * Modules :
 *   config.h        brochage, constantes, identifiants WiFi
 *   peripherals.*   capteur ABP, moteur, vanne, acquisition 100 Hz
 *   procedure.*     moteur de procedure (auto-diagnostic + test)
 *   web_server.*    serveur web / API REST
 *   www/            interface + generation du rapport PDF (jsPDF, cote navigateur)
 */
#include <Wire.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "config.h"
#include "peripherals.h"
#include "procedure.h"
#include "web_server.h"

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

  // WiFi : le banc cree son propre reseau (mode AP)
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("[BANC] WiFi AP ");
  Serial.print(AP_SSID);
  Serial.print(" - IP ");
  Serial.println(WiFi.softAPIP());

  // Tache d'acquisition sur le coeur 0 (moteur + ABP 100 Hz)
  startAcquisitionTask();

  // Serveur web sur le coeur 1
  setupServer();
}

void loop() {
  server.handleClient();
}
