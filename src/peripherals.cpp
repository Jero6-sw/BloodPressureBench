




#include "peripherals.h"
#include "config.h"
#include "valves.h"
#include <Wire.h>

BenchState BPB_BenchState;   // instance unique ; declaree extern dans peripherals.h

// ================= CAPTEUR ABP =================
// ABPDRRV040MG2A3 : sortie I2C 14 bits, 10%-90% de la pleine echelle.
// Transfer : counts = 1638 (0 kPa) .. 14745 (40 kPa), en kPa puis mmHg.
float readPressureMMHg() {
  Wire.requestFrom(ABP_ADDR, 2);
  if (Wire.available() < 2) { BPB_BenchState.abpOk = false; return BPB_BenchState.pressure; }
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
// Reference de phase came : appelee quand l'operateur a pose manuellement la
// came sur le repere FIN D'ASPIRATION (moteur OFF). Ensuite le comptage des
// pas (acquisitionTask) suit la phase de facon deterministe.
void motorSetPhaseRef()  { BPB_BenchState.motorPhase = 0; }
void motorStepToPhase(int32_t target) { BPB_BenchState.motorPhaseTarget = target; motorRun(true); }
bool  motorPhaseBusy()   { return BPB_BenchState.motorPhaseTarget >= 0; }
void motorRun(bool on)  { BPB_BenchState.motorOn  = on;  digitalWrite(PIN_EN,    on ? LOW  : HIGH); }
// Purge (V6) : passe par le module valves pour garder l'etat coherent (affichage SVG)
void valveSet(bool open){ BPB_BenchState.valveOpen = open; valveDrive(6, open); }

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
    if (BPB_BenchState.motorOn && us - lastStep >= STEP_PERIOD_US) {
      lastStep = us;
      digitalWrite(PIN_STEP, HIGH);
      digitalWrite(PIN_STEP, LOW);
      BPB_BenchState.motorPhase = (BPB_BenchState.motorPhase + 1) % MOTOR_CYCLE_USTEPS;
      if (BPB_BenchState.motorPhaseTarget >= 0 && BPB_BenchState.motorPhase == BPB_BenchState.motorPhaseTarget) {
        BPB_BenchState.motorPhaseTarget = -1;   // phase cible atteinte : stop propre
        motorRun(false);
      }
    }
    unsigned long ms = millis();
    if (ms - lastLog >= ACQ_PERIOD_MS) {          // 100 Hz
      lastLog = ms;
      // Capteur absent -> pas de lecture I2C : evite le flot d'erreurs
      // "i2c.master: I2C software timeout" a 100 Hz dans le moniteur/plotter.
      float p = BPB_BenchState.abpOk ? readPressureMMHg() : BPB_BenchState.pressure;
      BPB_BenchState.pressure = p;
      if (p < pMin) { pMin = p; tMin = ms; }
      if (p > pMax) { pMax = p; tMax = ms; }
      if (ms - lastPulse >= 1000) {               // metriques par pulse
        float rise = (tMax >= tMin) ? (tMax - tMin) : (1000 + tMax - tMin);
        BPB_BenchState.amp = pMax - pMin;
        BPB_BenchState.ratio = (rise > 0 && rise < 1000) ? rise / (1000 - rise) : 99;
        pMin = 1e9; pMax = -1e9;
        lastPulse = ms;
        // Diagnostics systeme (1 Hz) : temperature du chip + tension alim
        BPB_BenchState.chipTempC = temperatureRead();
#if PIN_VCC_SENSE >= 0
        BPB_BenchState.vccmV = analogReadMilliVolts(PIN_VCC_SENSE) * VCC_DIVIDER;
#else
        BPB_BenchState.vccmV = 0;
#endif
      }
    }
    vTaskDelay(1);
  }
}

void startAcquisitionTask() {
  xTaskCreatePinnedToCore(acquisitionTask, "acq", 4096, NULL, 2, NULL, 0);
}
