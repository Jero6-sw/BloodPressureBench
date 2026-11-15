#ifndef VALVES_H
#define VALVES_H

#include <Arduino.h>

// ---------- Gestion des electrovannes 2/2 (architecture v1.8 : 5 vannes) ----------
// Emule la matrice de configurations manuelles S1-S4 (README_FR 3 a) :
//   V1 (NO)  seringue <-> noeud M   : ouverte = S1-1 PULSATION (mesure)
//   V2 (NC)  seringue <-> branche clapets : ouverte = S1-2 POMPE (gonflage)
//   V3 (NC)  manometre <-> noeud M  : ouverte = S2-1 (lectures croisees / Boyle)
//   V5 (NO)  ligne CV2 <-> M        : ouverte = S4-1 (mode pompe uniquement)
//   V6 (NC)  purge atmosphere       : ouverte = degonflage
// V0 et V4 ne font plus partie du banc (v1.8) : si elles restent cablees, elles
// sont maintenues TRANSPARENTES (V0 fermee, V4 ouverte). La seringue monte en
// pression UNIQUEMENT par la branche clapets (V2+V5 ouvertes, V1 fermee) : en
// aller-retour, une poussee directe via V1 rendrait autant qu'elle ne donne.
// Pendant la mesure (V1 ouverte), V1/V5/V6 sont de-energisees -> zero watt.

enum BenchConfig {
  CFG_PUMP,          // gonflage (pompe)         : V2+V5 ouvertes
  CFG_MEASURE,       // pulsation / mesure       : V1 ouverte
  CFG_GAUGE,         // lectures croisees        : V3 ouverte
  CFG_BOYLE,         // Boyle (seringue port S2) : V3 ouverte
  CFG_PURGE,         // degonflage a 0 mmHg       : V6 ouverte
  CFG_STORAGE,       // stockage                  : tout ferme
  N_CONFIGS
};

void valvesInit();                     // broches + etat de securite (stockage)
void valvesApply(BenchConfig cfg);     // applique une configuration de la matrice
const char* valvesConfigName(BenchConfig cfg);
const char* valvesCurrentName();       // nom de la derniere configuration appliquee
bool valveIsEnergized(int v);          // 0..6
bool valveIsOpen(int v);               // 0..6 : etat OUVERT (affichage SVG)
void valveDrive(int v, bool open);     // pilotage individuel (test manuel)

// Auto-test d'isolement : ferme la vanne v, laisse evoluer la pression 60 s,
// mesure la pente de fuite (mmHg/min) cote ABP. Retourne false si fuite.
bool valveLeakTest(int v, float p0_mmHg, float* slopeOut);

#endif
