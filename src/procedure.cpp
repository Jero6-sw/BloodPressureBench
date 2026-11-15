#include "procedure.h"
#include "config.h"
#include "valves.h"

// ---------- Auto-diagnostic de session (v1.8 : 5 vannes + tests de sieges) ----------
// cfg = configuration d'electrovannes appliquee AUTOMATIQUEMENT au debut de l'etape.
const BPB_Step CALIB[] = {
  {"Montage banc",     "Banc ferme, EMBOUT DU BRAS BOUCHONE (obturateur Luer). Tourner la came a la main (moteur OFF) sur le repere FIN D'ASPIRATION, puis valider : le banc enregistre la reference de phase. Vannes : Stockage.", 'M', CFG_STORAGE},
  {"Etancheite banc (A0)", "Test des sieges 'bench zero' : montee a 150 mmHg via la branche clapets, arret auto du moteur en fin d'aspiration (seringue scellee a ~0), fermeture V2+V5, pente 60 s. Critere : S0 < 0,3 mmHg/min. Verdict affiche a la fin.", 'A', CFG_PUMP},
  {"Montage bras",     "Retirer l'obturateur, brancher le bras sur le filet, verrouiller le chuck. Valider pour lancer le test d'etancheite du bras.", 'M', CFG_STORAGE},
  {"Etancheite bras (B)", "Memme protocole que A0, bras connecte : fuite bras = S_total - S0 (mesure differentielle, la derive du capteur s'annule par soustraction). Critere : < 1,0 mmHg/min. Verdict affiche a la fin.", 'A', CFG_PUMP},
  {"Gonflage",         "Montee a 100 +/- 2 mmHg (ABP) par la branche clapets. Arret auto a la cible.", 'A', CFG_PUMP},
  {"Controle P_int",   "V3 OUVERTE (manometre connecte). Lire le manometre aneroide, saisir la valeur dans Rapport. Verifier 100 +/- 2 mmHg.", 'M', CFG_GAUGE},
  {"Zero etalons",     "V6 OUVERTE (purge, pression libre). Capture auto de l'offset electronique ABP ; verifier l'aiguille aneroide dans l'ovale zero. Purge fermee a la fin de l'etape.", 'A', CFG_PURGE},
  {"Linearite",        "V3 OUVERTE. Paliers 50/100/150 mmHg (monter avec le moteur du menu Test manuel), saisir chaque lecture aneroide dans Rapport. Signature D(P)=aneroide-ABP.", 'M', CFG_GAUGE},
  {"Volume+Gain (Boyle)", "V3 OUVERTE, branche clapets isolee. Seringue 10 mL sur le port S2 (cote manometre) : injecter 10 mL, VALIDER (deltaP -> V et gain ABP), retirer la seringue.", 'M', CFG_BOYLE},
  {"Amplitude",        "V1 OUVERTE (pulsation directe), branche clapets isolee. Moteur ON : lecture auto amplitude + forme sur 4 s. Ajuster la butee si hors cible.", 'A', CFG_MEASURE},
};
const int N_CALIB = sizeof(CALIB) / sizeof(CALIB[0]);

// ---------- Etapes de la procedure de test (README_FR 8 b) ----------
const BPB_Step TESTS[] = {
  {"Pret",    "V1 OUVERTE (pulsation). Moteur ON (60 bpm). Lancer la mesure sur le tensiometre.", 'M', CFG_MEASURE},
  {"Mesures", "Saisir la valeur affichee par le tensiometre - x5, 30 s d'intervalle.", 'M', CFG_MEASURE},
  {"Fin",     "V6 OUVERTE (degonflage a 0 mmHg), puis Stockage (tout isole). Deverrouiller le chuck.", 'M', CFG_PURGE},
};
const int N_TESTS = sizeof(TESTS) / sizeof(TESTS[0]);

// ---------- Maintenance mensuelle : branche clapets (v1.8) ----------
const BPB_Step MAINT[] = {
  {"Preparation",  "EMBOUT DU BRAS BOUCHONE, bras debranche. Valider pour lancer le test mensuel de la branche clapets.", 'M', CFG_STORAGE},
  {"Branche clapets (A1)", "Montee a 150 mmHg, fermeture V2+V5 (branche scellee a ~150), ouverture V1 (retour au circuit), 10 s de stabilisation, pente 60 s SIGNEE. > +0,3 : fuite siege V2 ou V5 ; < -0,3 : fuite externe (refaire A0).", 'A', CFG_PUMP},
};
const int N_MAINT = sizeof(MAINT) / sizeof(MAINT[0]);

// ---------- Sequence en cours (privee a ce fichier) ----------
static const BPB_Step* curTable = nullptr;
static int             curLen   = 0;

static const BPB_Step* getStep(int i) { return (curTable && i < curLen) ? &curTable[i] : nullptr; }

// ---------- Partie automatique : MACHINE A ETATS NON BLOQUANTE ----------
static uint32_t autoT0    = 0;    // debut de la sous-phase
static float    autoP0    = 0;    // pression de reference (pentes)
static char     autoPhase = 0;   // 0 = rien ; 1..4 = sous-phases en cours

static void autoFinish(const char* msg) {
  motorRun(false);                 // etat de securite systematique
  valveSet(false);                 // purge fermee
  strlcpy(BPB_BenchState.approvalMsg, msg, sizeof(BPB_BenchState.approvalMsg));
  BPB_BenchState.waitingApproval = true;
  autoPhase = 0;
}

void procedureStart(const BPB_Step* tbl, int len) {
  curTable = tbl; curLen = len; BPB_BenchState.step = 0;
  autoPhase = 0;
  procedureRunAuto(0);
}

void procedureAdvance() {
  if (!curTable) return;
  // Validation de l'etape 0 = came posee sur le repere fin d'aspiration
  if ((curTable == CALIB || curTable == MAINT) && BPB_BenchState.step == 0)
    motorSetPhaseRef();
  BPB_BenchState.step++;
  if (BPB_BenchState.step >= curLen) { BPB_BenchState.step = curLen; return; }
  procedureRunAuto(BPB_BenchState.step);
}

void procedureRunAuto(int idx) {
  const BPB_Step* st = getStep(idx);
  if (!st) return;
  strlcpy(BPB_BenchState.approvalMsg, st->instr, sizeof(BPB_BenchState.approvalMsg));
  if (st->cfg >= 0 && st->cfg < N_CONFIGS) valvesApply((BenchConfig)st->cfg);
  if (st->type == 'M') { BPB_BenchState.waitingApproval = true; return; }
  if (curTable == TESTS)  { BPB_BenchState.waitingApproval = true; return; }
  if (!BPB_BenchState.abpOk) {
    autoFinish("Capteur ABP absent : etape annulee (brancher le capteur puis relancer).");
    return;
  }
  autoT0 = millis();
  autoPhase = 1;
  if (curTable == MAINT) {                    // A1 : montee a 150 par la pompe
    if (idx == 1) motorRun(true);
    return;
  }
  switch (idx) {
    case 1: case 3:                           // A0 (sieges) / B (bras) : pompe a 150
    case 4:                                   // gonflage a 100
    case 9:                                   // amplitude : pulsation 4 s
      motorRun(true);
      break;
    case 6:                                   // zero : purge ouverte par CFG_PURGE
      break;
    default:
      autoPhase = 0;
      BPB_BenchState.waitingApproval = true;
      break;
  }
}

// Avance l'etape automatique en cours (appele dans loop(), jamais bloquant).
void procedureTick() {
  if (autoPhase == 0 || curTable == TESTS) return;
  int      idx = BPB_BenchState.step;
  uint32_t dT  = millis() - autoT0;
  char     msg[160];

  // ---- A0 / B : montee 150, arret sur phase, isolement, pente 60 s ----
  if (curTable == CALIB && (idx == 1 || idx == 3)) {
    if (autoPhase == 1) {                                  // montee a 150
      if (BPB_BenchState.pressure >= P_LEAKTEST_MMHG - 0.5f) {
        motorStepToPhase(0);                               // arret fin d'aspiration (seringue ~0)
        autoT0 = millis(); autoPhase = 2;
      } else if (dT > 120000UL)
        autoFinish("ECHEC : 150 mmHg non atteints en 120 s (fuite ? branche clapets ?). Verifier puis valider.");
    } else if (autoPhase == 2) {                           // attente de la phase cible
      if (!motorPhaseBusy()) {
        valvesApply(CFG_STORAGE);                          // V2+V5 fermees : seringue et branche scellees
        autoT0 = millis(); autoPhase = 3;
      } else if (dT > 5000UL) {
        motorRun(false);
        autoFinish("ECHEC : phase de came non atteinte (pas perdus ?). Realigner la came au repere puis valider.");
      }
    } else if (autoPhase == 3) {                           // stabilisation 3 s
      if (dT >= 3000UL) { autoP0 = BPB_BenchState.pressure; autoT0 = millis(); autoPhase = 4; }
    } else if (autoPhase == 4 && dT >= 60000UL) {          // pente 60 s
      float slope = BPB_BenchState.pressure - autoP0;
      if (idx == 1) {
        BPB_BenchState.seatS0 = slope;
        snprintf(msg, sizeof(msg), "A0 bench zero : %.2f mmHg/min (sieges V1+V3+V6). %s",
                 slope, (slope < 0.3f) ? "CONFORME (<0,3) - valider." : "NON CONFORME : diagnostiquer les sieges, valider pour continuer.");
      } else {
        BPB_BenchState.seatArm = slope;
        if (BPB_BenchState.seatS0 < 0) {
          snprintf(msg, sizeof(msg), "B : %.2f mmHg/min, mais A0 absent - relancer l'auto-diagnostic depuis le debut.", slope);
        } else {
          BPB_BenchState.armLeak = slope - BPB_BenchState.seatS0;
          snprintf(msg, sizeof(msg), "Fuite bras = %.2f - %.2f = %.2f mmHg/min. %s",
                   slope, BPB_BenchState.seatS0, BPB_BenchState.armLeak,
                   (BPB_BenchState.armLeak < LEAK_SLOPE_MAX) ? "CONFORME (<1,0) - valider." : "NON CONFORME : bras/brassard a verifier, valider pour continuer.");
        }
      }
      autoFinish(msg);
    }
    return;
  }

  // ---- A1 (MAINT) : branche clapets par ouverture de V1 ----
  if (curTable == MAINT && idx == 1) {
    if (autoPhase == 1) {                                  // montee a 150
      if (BPB_BenchState.pressure >= P_LEAKTEST_MMHG - 0.5f) {
        motorRun(false);
        valvesApply(CFG_MEASURE);                          // V2+V5 fermees, V1 ouverte : retour au circuit
        autoT0 = millis(); autoPhase = 2;
      } else if (dT > 120000UL)
        autoFinish("ECHEC : 150 mmHg non atteints en 120 s. Verifier la branche clapets puis valider.");
    } else if (autoPhase == 2) {                           // stabilisation 10 s
      if (dT >= 10000UL) { autoP0 = BPB_BenchState.pressure; autoT0 = millis(); autoPhase = 3; }
    } else if (autoPhase == 3 && dT >= 60000UL) {          // pente signee 60 s
      float slope = BPB_BenchState.pressure - autoP0;
      BPB_BenchState.seatBranch = slope;
      const char* verdict = (slope > 0.3f)  ? "fuite siege V2 ou V5 (diagnostiquer)."
                       : (slope < -0.3f) ? "fuite externe (V3/V6) - refaire A0."
                       : "branche clapets ETANCHE.";
      snprintf(msg, sizeof(msg), "A1 branche clapets : %+.2f mmHg/min. %s Valider.", slope, verdict);
      autoFinish(msg);
    }
    return;
  }

  if (curTable != CALIB) return;
  switch (idx) {
    case 4:   // gonflage : arret a 100 +/- 2 mmHg, echec a 120 s
      if (BPB_BenchState.pressure >= P_TARGET_MMHG - P_TOLERANCE_MMHG)
        autoFinish("Gonflage termine a la cible (100 mmHg). Verifier P_int puis valider.");
      else if (dT > 120000UL)
        autoFinish("ECHEC gonflage : cible non atteinte en 120 s (fuite ? moteur ?). Verifier puis valider.");
      break;

    case 6:   // zero : capture apres 3 s de purge ouverte
      if (autoPhase == 1 && dT >= 3000UL) {
        BPB_BenchState.zeroOffset = BPB_BenchState.pressure;
        autoFinish("Zero ABP capture. Verifier l'aiguille aneroide dans l'ovale zero puis valider.");
      }
      break;

    case 9:   // amplitude + forme : 4 s de pulsation
      if (dT >= 4000UL)
        autoFinish("Amplitude et forme relevees (voir bandeau). Ajuster la butee si hors cible puis valider.");
      break;
  }
}
