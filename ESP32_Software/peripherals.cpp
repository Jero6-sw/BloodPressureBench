#include "peripherals.h"
#include "config.h"
#include <Wire.h>

BenchState S;

// ================= CAPTEUR ABP =================
// ABPDRRV040MG2A3 : sortie I2C 14 bits, 10%-90% de la pleine echelle.
// Transfer : counts = 1638 (0 kPa) .. 14745 (40 kPa), en kPa puis mmHg.
float readPressureMMHg() {
  Wire.requestFrom(ABP_ADDR, 2);
  if (Wire.available() < 2) { S.abpOk = false; return S.pressure; }
  uint8_t msb = Wire.read(), lsb = Wire.read();
  uint16_t raw = ((uint16_t)(msb & 0x3F) << 8) | lsb;
  float kpa = (raw - 1638.0f) * ABP_FULLSCALE_KPA / 13107.0f;
  return kpa * KPA2MMHG;
}

bool abpPresent() {
  Wire.beginTransmission(ABP_ADDR);
  return (Wire.endTransmission() == 0);
}

// ================= ACTIONNEURS =================
void motorRun(bool on)  { S.motorOn  = on;  digitalWrite(PIN_EN,    on ? LOW  : HIGH); }
void valveSet(bool open){ S.valveOpen = open; digitalWrite(PIN_VALVE, open ? HIGH : LOW); }

// ================= ACQUISITION (coeur 0) =================
// - genere les pas du moteur a 313 us (60 tr/min = 60 bpm) quand le moteur tourne ;
// - lit le capteur ABP a 100 Hz ;
// - calcule par periode de 1 s : amplitude crete-a-crete et rapport
//   temps de montee / temps de descente du pulse (metrique de "carrete").
//   ratio > 1,8 -> signal trop carre -> mousse EPDM (voir README_FR 1 b).
void acquisitionTask(void*) {
  unsigned long lastStep = 0, lastLog = 0, lastPulse = millis();
  float pMin = 1e9, pMax = -1e9;
  unsigned long tMin = 0, tMax = 0;
  for (;;) {
    unsigned long us = micros();
    if (S.motorOn && us - lastStep >= STEP_PERIOD_US) {
      lastStep = us;
      digitalWrite(PIN_STEP, HIGH);
      digitalWrite(PIN_STEP, LOW);
    }
    unsigned long ms = millis();
    if (ms - lastLog >= ACQ_PERIOD_MS) {          // 100 Hz
      lastLog = ms;
      float p = readPressureMMHg();
      S.pressure = p;
      if (p < pMin) { pMin = p; tMin = ms; }
      if (p > pMax) { pMax = p; tMax = ms; }
      if (ms - lastPulse >= 1000) {               // metriques par pulse
        float rise = (tMax >= tMin) ? (tMax - tMin) : (1000 + tMax - tMin);
        S.amp = pMax - pMin;
        S.ratio = (rise > 0 && rise < 1000) ? rise / (1000 - rise) : 99;
        pMin = 1e9; pMax = -1e9;
        lastPulse = ms;
      }
    }
    vTaskDelay(1);
  }
}

void startAcquisitionTask() {
  xTaskCreatePinnedToCore(acquisitionTask, "acq", 4096, NULL, 2, NULL, 0);
}
