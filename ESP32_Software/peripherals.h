#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <Arduino.h>

// ---------- Etat global du banc (partage par tous les modules) ----------
struct BenchState {
  // Mesures temps reel
  float pressure = 0;                 // mmHg (capteur ABP)
  float amp = 0, ratio = 1;           // dernier pulse : crete-a-crete + rapport montee/descente
  // Actionneurs
  bool  motorOn = false, valveOpen = false, abpOk = false;
  // Procedure
  int   step = 0;
  bool  waitingApproval = false;
  char  approvalMsg[160];
  // Resultats de session (alimentent le rapport PDF)
  float leakSlope = 0, leakSlopeArm = 0, boyleV = 0;
  float pReads[3] = {0, 0, 0};         // lectures aneroide aux 3 paliers 50/100/150
  float displayed[5] = {0, 0, 0, 0, 0}; // valeurs affichees par le tensiometre
  float tempC = 21;
  char  operatorName[32] = "", deviceSN[32] = "", fwDevice[24] = "";
  float zeroOffset = 0, gainErrPct = 0;      // etapes de departage de derive (zero air libre, gain Boyle)
  char  patientName[32] = "";               // patient selectionne en debut de test (liste predefinie config.h)
};

extern BenchState S;

// ---------- Capteur ABP ----------
float readPressureMMHg();             // lecture I2C brute convertie en mmHg
bool  abpPresent();                   // test de reponse I2C

// ---------- Actionneurs ----------
void motorRun(bool on);               // enable du TMC2209
void valveSet(bool open);             // electrovanne de purge

// ---------- Acquisition ----------
void acquisitionTask(void*);          // tache coeur 0 : pas du moteur a 313 us + ABP a 100 Hz
void startAcquisitionTask();           // creation de la tache (appelee dans setup())

#endif
