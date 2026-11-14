

#include "procedure.h"
#include "config.h"

// ---------- Etapes de l'auto-diagnostic (README_FR 8 a) ----------
const BPB_Step CALIB[] = {
  {"Montage",         "Bras sur filet, chuck verrouille. S3 (1) OUVERT, S1 (2) POMPE, S4 (1) OUVERT, S2 (2) FERME.", 'M'},
  {"Gonflage",        "Moteur ON en S1 (2)/S4 (1) - arret auto a 100 +/- 2 mmHg (ABP).", 'A'},
  {"Controle P_int",  "S2 (1) OUVERT, lire le manometre aneroide, saisir la valeur, S2 (2) FERME.", 'M'},
  {"Etancheite banc", "S3 (2) FERME : montee a 150 mmHg, pente 60 s (< 1 mmHg/min).", 'A'},
  {"Etancheite bras", "S1 (3) 45 degres + S3 (1) OUVERT : bras seul, pente 60 s (< 1 mmHg/min).", 'A'},
  {"Zero etalons",    "Purge (V6) OUVERTE : capture auto de l'offset electronique ABP ; verifier l'aiguille aneroide dans l'ovale zero. Fermer la purge.", 'A'},
  {"Linearite",       "Paliers 50/100/150 : a chaque palier S2 (1) OUVERT, saisir la lecture aneroide. Signature D(P)=aneroide-ABP : constante=offset, proportionnelle=gain, non lineaire=mecanisme aneroide.", 'M'},
  {"Volume+Gain (Boyle)", "S1 (3) + S2 (1) + S3 (1) + S4 (2), seringue 10 mL sur port S2 : injecter 10 mL, [VALIDER] (deltaP -> V et gain ABP : deltaP attendu = P*10/(V-10), ecart > 1,5 mmHg = derive electronique), retirer la seringue, S2 (2) FERME.", 'M'},
  {"Amplitude",       "S1 (1) PULSATION + S4 (2) FERME, moteur ON : lecture auto amplitude + forme sur 4 s. Ajuster la butee si hors cible.", 'A'},
};
const int N_CALIB = sizeof(CALIB) / sizeof(CALIB[0]);

// ---------- Etapes de la procedure de test (README_FR 8 b) ----------
const BPB_Step TESTS[] = {
  {"Pret",    "S1 (1) PULSATION + S4 (2) FERME, S3 (1) OUVERT, moteur ON (60 bpm). Lancer la mesure sur le tensiometre.", 'M'},
  {"Mesures", "Saisir la valeur affichee par le tensiometre - x5, 30 s d'intervalle.", 'M'},
  {"Fin",     "Moteur OFF, S1 (3), vanne ouverte jusqu'a 0 mmHg, S3 (2), S4 (2), deverrouiller le chuck.", 'M'},
};
const int N_TESTS = sizeof(TESTS) / sizeof(TESTS[0]);

// ---------- Sequence en cours (privee a ce fichier : pas d'export dans procedure.h) ----------
static const BPB_Step* curTable = nullptr;
static int             curLen   = 0;

static const BPB_Step* getStep(int i) { return (curTable && i < curLen) ? &curTable[i] : nullptr; }
static int             getStepCount() { return curLen; }

void procedureStart(const BPB_Step* tbl, int len) {
  curTable = tbl; curLen = len; BPB_BenchState.step = 0;
  procedureRunAuto(0);
}

void procedureAdvance() {
  if (!curTable) return;
  BPB_BenchState.step++;
  if (BPB_BenchState.step >= curLen) { BPB_BenchState.step = curLen; return; }   // sequence terminee
  procedureRunAuto(BPB_BenchState.step);
}

// ---------- Partie automatique des etapes ----------
// NOTE : les attentes ci-dessous sont ecrites de facon lisible (boucles + delay)
// pour la lisibilite. En production, les implementer comme machine a etats
// non bloquante (meme modele que acquisitionTask) afin que le serveur web
// reste reactif pendant les mesures de 60 s.
void procedureRunAuto(int idx) {
  const BPB_Step* st = getStep(idx);
  if (!st) return;
  strlcpy(BPB_BenchState.approvalMsg, st->instr, sizeof(BPB_BenchState.approvalMsg));
  if (st->type == 'M') { BPB_BenchState.waitingApproval = true; return; }   // l'operateur doit confirmer

  // --- actions automatiques ---
  if (curTable == CALIB) {
    switch (idx) {
      case 1:  // gonflage a 100 mmHg
        motorRun(true);
        while (BPB_BenchState.pressure < P_TARGET_MMHG - P_TOLERANCE_MMHG) delay(5);
        motorRun(false);
        break;
      case 3:  // etancheite banc seul : montee a 150 mmHg puis pente 60 s
        motorRun(true);
        while (BPB_BenchState.pressure < P_LEAKTEST_MMHG - 0.5f) delay(5);
        motorRun(false);
        {
          float p0 = BPB_BenchState.pressure;
          delay(60000);
          BPB_BenchState.leakSlope = BPB_BenchState.pressure - p0;          // mmHg/min (seuil : < 1)
        }
        break;
      case 4:  // etancheite bras seul : pente 60 s (S1 45 deg + S3 ouvert cote operateur)
        {
          float p0 = BPB_BenchState.pressure;
          delay(60000);
          BPB_BenchState.leakSlopeArm = BPB_BenchState.pressure - p0;
        }
        break;
      case 5:  // zero a l'air libre : purge ouverte, capture de l'offset ABP (etape 1 du departage)
        valveSet(true);                        // purge ouverte -> pression atmospherique
        delay(3000);                           // stabilisation
        BPB_BenchState.zeroOffset = BPB_BenchState.pressure;             // offset electronique direct (mmHg), mesure absolue
        valveSet(false);                       // l'operateur verifie l'ovale zero de l'aneroide puis valide
        break;
      case 8:  // amplitude + forme sur 4 s de pulsation
        motorRun(true);
        delay(4000);
        motorRun(false);
        break;
    }
  }
  BPB_BenchState.waitingApproval = true;   // l'operateur valide le resultat de l'action auto
}
