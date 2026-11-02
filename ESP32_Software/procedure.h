#ifndef PROCEDURE_H
#define PROCEDURE_H

#include "peripherals.h"

// ---------- Moteur de procedure ----------
// Chaque etape est AUTO (le banc agit) ou MANUEL (attente d'approbation
// bloquante de l'operateur via /api/approve).
// Les positions S1/S2/S3/S4 utilisent la numerotation officielle du
// document d'architecture (README_FR, section 3 a) :
//   S1 : 1 PULSATION / 2 POMPE / 3 45 degres ; S2/S3/S4 : 1 OUVERT / 2 FERME.
struct Step { const char* title; const char* instr; char type; };  // A=auto, M=manuel

extern const Step CALIB[];   // auto-diagnostic (README_FR 8 a)
extern const Step TESTS[];   // procedure de test (README_FR 8 b)
extern const int   N_CALIB, N_TESTS;

// Sequence en cours
extern const Step* curTable;
extern int        curLen;
extern const Step* getStep(int i);
extern int        getStepCount();

void procedureStart(const Step* tbl, int len);  // debut de sequence
void procedureAdvance();                         // approbation recue -> etape suivante
void procedureRunAuto(int idx);                  // partie automatique de l'etape idx

#endif
