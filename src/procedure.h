



#ifndef PROCEDURE_H
#define PROCEDURE_H

#include "peripherals.h"

// ---------- Moteur de procedure ----------
// Chaque etape est AUTO (le banc agit) ou MANUEL (attente d'approbation
// bloquante de l'operateur via /api/approve).
// Les positions S1/S2/S3/S4 utilisent la numerotation officielle du
// document d'architecture (README_FR, section 3 a) :
//   S1 : 1 PULSATION / 2 POMPE ; S2 : 1 OUVERT / 2 FERME.
// v1.8 : 5 vannes (V1,V2,V3,V5,V6) - V0/V4 retirees ; l'etancheite banc/bras
// passe par les tests de sieges A0/A1 et la fuite bras par difference (B).
struct BPB_Step { const char* title; const char* instr; char type; signed char cfg; };
// type : A=auto, M=manuel ; cfg : configuration d'electrovannes (valves.h) appliquee au debut de l'etape, -1 = inchangee

extern const BPB_Step CALIB[];   // auto-diagnostic de session (10 etapes, v1.8)
extern const BPB_Step TESTS[];   // procedure de test (README_FR 8 b)
extern const BPB_Step MAINT[];   // maintenance mensuelle : test de la branche clapets (v1.8)
extern const int   N_CALIB, N_TESTS, N_MAINT;

// Sequence en cours : curTable, curLen, getStep() et getStepCount() sont
// PRIVES a procedure.cpp (static) - utilises uniquement en interne.

void procedureStart(const BPB_Step* tbl, int len);  // debut de sequence
void procedureAdvance();                         // approbation recue -> etape suivante
void procedureRunAuto(int idx);                  // lancement de l'etape auto idx (phase 1)
void procedureTick();                           // avance l'etape auto en cours (NON bloquant, a appeler dans loop())

#endif
