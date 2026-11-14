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

// ---------- Partie automatique : MACHINE A ETATS NON BLOQUANTE ----------
// Aucun delay()/while bloquant : procedureTick() (appele dans loop()) fait
// avancer les phases. Le serveur web et l'interface restent reactifs pendant
// les mesures de 60 s, et un capteur absent ne fige plus l'ESP32.
// Phases : 0 = rien a faire ; 1/2 = sous-phases de l'etape auto en cours.
static uint32_t autoT0    = 0;    // debut de la sous-phase
static float    autoP0    = 0;    // pression de reference (pentes de fuite)
static char     autoPhase = 0;

static void autoFinish(const char* msg) {
  motorRun(false);                 // etat de securite systematique
  valveSet(false);
  strlcpy(BPB_BenchState.approvalMsg, msg, sizeof(BPB_BenchState.approvalMsg));
  BPB_BenchState.waitingApproval = true;   // l'operateur valide le resultat
  autoPhase = 0;
}

void procedureStart(const BPB_Step* tbl, int len) {
  curTable = tbl; curLen = len; BPB_BenchState.step = 0;
  autoPhase = 0;
  procedureRunAuto(0);
}

void procedureAdvance() {
  if (!curTable) return;
  BPB_BenchState.step++;
  if (BPB_BenchState.step >= curLen) { BPB_BenchState.step = curLen; return; }   // sequence terminee
  procedureRunAuto(BPB_BenchState.step);
}

// Lance l'etape automatique idx : initialise la phase 1 (actions instantanees
// uniquement), la suite est pilotee par procedureTick().
void procedureRunAuto(int idx) {
  const BPB_Step* st = getStep(idx);
  if (!st) return;
  strlcpy(BPB_BenchState.approvalMsg, st->instr, sizeof(BPB_BenchState.approvalMsg));
  if (st->type == 'M') { BPB_BenchState.waitingApproval = true; return; }   // l'operateur doit confirmer

  if (curTable != CALIB) { BPB_BenchState.waitingApproval = true; return; }
  autoT0 = millis();
  autoPhase = 1;
  switch (idx) {
    case 1:                                   // gonflage a 100 mmHg
    case 3:                                   // montee a 150 mmHg puis pente
      if (!BPB_BenchState.abpOk) {            // capteur absent : pas de boucle infinie
        autoFinish("Capteur ABP absent : etape d'etonnage/gonflage annulee (brancher le capteur puis relancer la procedure).");
        return;
      }
      motorRun(true);
      break;
    case 4:                                   // pente bras seul : reference immediatement
      autoP0 = BPB_BenchState.pressure;
      break;
    case 5:                                   // zero a l'air libre : purge ouverte 3 s
      valveSet(true);
      break;
    case 8:                                   // pulsation 4 s
      motorRun(true);
      break;
    default:                                  // etape auto sans action temporisee
      autoPhase = 0;
      BPB_BenchState.waitingApproval = true;
      break;
  }
}

// Avance l'etape automatique en cours (a appeler dans loop(), jamais dans
// une tache SPI/I2C). Gerer les depassements de delai -> jamais de blocage.
void procedureTick() {
  if (autoPhase == 0 || curTable != CALIB) return;
  int      idx = BPB_BenchState.step;
  uint32_t dT  = millis() - autoT0;

  switch (idx) {
    case 1:   // gonflage : arret a 100 +/- 2 mmHg, echec a 120 s
      if (BPB_BenchState.pressure >= P_TARGET_MMHG - P_TOLERANCE_MMHG)
        autoFinish("Gonflage termine a la cible (100 mmHg). Verifier P_int puis valider.");
      else if (dT > 120000UL)
        autoFinish("ECHEC gonflage : cible non atteinte en 120 s (fuite ? moteur ?). Verifier puis valider.");
      break;

    case 3:   // etancheite banc : montee a 150 puis pente 60 s
      if (autoPhase == 1) {
        if (BPB_BenchState.pressure >= P_LEAKTEST_MMHG - 0.5f) {
          motorRun(false);
          autoP0 = BPB_BenchState.pressure;
          autoT0 = millis();
          autoPhase = 2;
        } else if (dT > 120000UL) {
          autoFinish("ECHEC : 150 mmHg non atteints en 120 s (fuite ?). Verifier puis valider.");
        }
      } else if (autoPhase == 2 && dT >= 60000UL) {
        BPB_BenchState.leakSlope = BPB_BenchState.pressure - autoP0;   // mmHg/min
        autoFinish("Pente banc mesuree (voir statut). Seuil < 1 mmHg/min - verifier puis valider.");
      }
      break;

    case 4:   // etancheite bras seul : pente 60 s
      if (autoPhase == 1) {                   // reference prise au lancement
        autoT0 = millis();
        autoPhase = 2;
      } else if (autoPhase == 2 && dT >= 60000UL) {
        BPB_BenchState.leakSlopeArm = BPB_BenchState.pressure - autoP0;
        autoFinish("Pente bras mesuree (voir statut). Seuil < 1 mmHg/min - verifier puis valider.");
      }
      break;

    case 5:   // zero : capture apres 3 s de purge ouverte
      if (autoPhase == 1 && dT >= 3000UL) {
        BPB_BenchState.zeroOffset = BPB_BenchState.pressure;   // offset electronique (mmHg)
        autoFinish("Zero ABP capture. Verifier l'aiguille aneroide dans l'ovale zero puis valider.");
      }
      break;

    case 8:   // amplitude + forme : 4 s de pulsation
      if (dT >= 4000UL)
        autoFinish("Amplitude et forme relevees (voir carte statut). Ajuster la butee si hors cible puis valider.");
      break;
  }
}
