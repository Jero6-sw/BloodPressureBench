#ifndef VALVES_H
#define VALVES_H

#include <Arduino.h>

// ---------- Gestion des electrovannes 2/2 (variante automatisee) ----------
// Emule la matrice de configurations manuelles S1-S4 (README_FR 3 a) :
//   V0 (NO)  serie sur l'alimentation seringue : fermee = S1 45 degres (tout ferme)
//   V1 (NO)  seringue <-> noeud M              : ouverte = S1 1 PULSATION
//   V2 (NC)  seringue <-> branche clapets      : ouverte = S1 2 POMPE
//   V3 (NC)  manometre <-> noeud M             : ouverte = S2 1 (lectures croisees)
//   V4 (NC)  bras <-> banc                     : ouverte = S3 1 (usage normal)
//   V5 (NO)  ligne CV2 <-> M                   : ouverte = S4 1 (mode pompe uniquement)
//   V6 (NC)  purge atmosphere                  : ouverte = degonflage
//
// Une vanne NO doit etre ENERGISEE pour se fermer : pendant la mesure
// (PULSATION), V0/V1/V5/V6 sont de-energisees -> zero watt, pas d'echauffement
// pres de la ligne de mesure.

enum BenchConfig {
  CFG_INFLATE,        // gonflage (pompe)          : S1 2, S2 2, S3 1, S4 1
  CFG_MEASURE,        // pulsation / mesure        : S1 1, S2 2, S3 1, S4 2
  CFG_BENCH_ONLY,     // test seringue<->capteur   : S1 1, S2 2, S3 2, S4 2
  CFG_ARM_ONLY,       // etancheite bras seul      : S1 3, S2 2, S3 1, S4 2
  CFG_GAUGE,          // lectures croisees         : S2 1, S3 1
  CFG_BOYLE,          // Boyle, seringue cal. S2   : S1 3, S2 1, S3 1, S4 2
  CFG_STORAGE,        // stockage                  : tout ferme/isole
  CFG_PURGE,          // degonflage a 0 mmHg
  N_CONFIGS
};

void valvesInit();                     // broches + etat de securite (stockage)
void valvesApply(BenchConfig cfg);     // applique une configuration de la matrice
const char* valvesConfigName(BenchConfig cfg);
bool valveIsEnergized(int v);          // 0..6

// Auto-test d'isolement : ferme la vanne v, gonfle le segment amont a p0,
// mesure la pente de fuite (mmHg/min) cote amont. Retourne false si fuite.
bool valveLeakTest(int v, float p0_mmHg, float* slopeOut);

#endif
