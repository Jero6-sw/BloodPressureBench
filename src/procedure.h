
#ifndef PROCEDURE_H
#define PROCEDURE_H

#include "peripherals.h"

// ---------- Moteur de procedure ----------
// Chaque etape est AUTO (le banc agit) ou MANUEL (attente d'approbation
// bloquante de l'operateur via /api/approve).
// Les positions S1/S2/S3/S4 utilisent la numerotation officielle du
// document d'architecture (README_FR, section 3 a) :
//   S1 : 1 PULSATION / 2 POMPE / 3 45 degres ; S2/S3/S4 : 1 OUVERT / 2 FERME.
struct BPB_Step { const char* title; const char* instr; char type; };  // A=auto, M=manuel

extern const BPB_Step CALIB[];   // auto-diagnostic (README_FR 8 a)
extern const BPB_Step TESTS[];   // procedure de test (README_FR 8 b)
extern const int   N_CALIB, N_TESTS;

// Sequence en cours : curTable, curLen, getStep() et getStepCount() sont
// PRIVES a procedure.cpp (static) - utilises uniquement en interne.

void procedureStart(const BPB_Step* tbl, int len);  // debut de sequence
void procedureAdvance();                         // approbation recue -> etape suivante
void procedureRunAuto(int idx);                  // partie automatique de l'etape idx

#endif
