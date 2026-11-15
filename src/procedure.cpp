#include "procedure.h"
#include "config.h"
#include "valves.h"

// ---------- Etapes de l'auto-diagnostic (README_FR 8 a) ----------
// cfg = configuration d'electrovannes appliquee AUTOMATIQUEMENT au debut de
// l'etape (V0-V6) : l'operateur n'a plus de manip pneumatique manuelle.
const BPB_Step CALIB[] = {
  {"Montage",         "Bras sur filet, chuck verrouille. Vannes automatiques : le banc passe en configuration Gonflage (V0,V2,V4,V5 ouvertes).", 'M', CFG_INFLATE},
  {"Gonflage",        "Moteur ON en configuration Gonflage - arret auto a 100 +/- 2 mmHg (ABP).", 'A', CFG_INFLATE},
  {"Controle P_int",  "Vannes automatiques : Lecteurs croisees (V3+V4 ouvertes). Lire le manometre aneroide, saisir la valeur.", 'M', CFG_GAUGE},
  {"Etancheite banc", "Vannes automatiques : Banc seul (V4 fermee). Montee a 150 mmHg puis pente 60 s (< 1 mmHg/min).", 'A', CFG_BENCH_ONLY},
  {"Etancheite bras", "Vannes automatiques : Bras seul (V0..V3,V5 fermees, V4 ouverte). Pente 60 s (< 1 mmHg/min).", 'A', CFG_ARM_ONLY},
  {"Zero etalons",    "Vannes automatiques : Purge (V6 OUVERTE). Capture auto de l'offset electronique ABP ; verifier l'aiguille aneroide dans l'ovale zero. La purge se ferme a la fin de l'etape.", 'A', CFG_PURGE},
  {"Linearite",       "Vannes automatiques : Lectures croisees (V3+V4). Paliers 50/100/150 : saisir la lecture aneroide a chaque palier. Signature D(P)=aneroide-ABP.", 'M', CFG_GAUGE},
  {"Volume+Gain (Boyle)", "Vannes automatiques : Boyle (V3+V4 ouvertes, V0 ferme). Seringue 10 mL sur port S2 : injecter 10 mL, [VALIDER] (deltaP -> V et gain ABP), retirer la seringue.", 'M', CFG_BOYLE},
  {"Amplitude",       "Vannes automatiques : Pulsation/mesure (V1 ouverte, V5 fermee). Moteur ON : lecture auto amplitude + forme sur 4 s. Ajuster la butee si hors cible.", 'A', CFG_MEASURE},
};
const int N_CALIB = sizeof(CALIB) / sizeof(CALIB[0]);

// ---------- Etapes de la procedure de test (README_FR 8 b) ----------
const BPB_Step TESTS[] = {
  {"Pret",    "Vannes automatiques : Pulsation/mesure. Moteur ON (60 bpm). Lancer la mesure sur le tensiometre.", 'M', CFG_MEASURE},
  {"Mesures", "Saisir la valeur affichee par le tensiometre - x5, 30 s d'intervalle.", 'M', CFG_MEASURE},
  {"Fin",     "Vannes automatiques : Purge (degonflage a 0 mmHg), puis Stockage (tout isole). Deverrouiller le chuck.", 'M', CFG_PURGE},
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
static uint32_t autoT0    = 0;    // debut de la sous-phase
static float    autoP0    = 0;    // pression de reference (pentes de fuite)
static char     autoPhase = 0;   // 0 = rien ; 1/2 = sous-phases en cours

static void autoFinish(const char* msg) {
  motorRun(false);                 // etat de securite systematique
  valveSet(false);                 // purge fermee
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

// Lance l'etape idx : applique la configuration d'electrovannes de l'etape,
// initialise la phase 1 (actions instantanees uniquement), la suite est
// pilotee par procedureTick().
void procedureRunAuto(int idx) {
  const BPB_Step* st = getStep(idx);
  if (!st) return;
  strlcpy(BPB_BenchState.approvalMsg, st->instr, sizeof(BPB_BenchState.approvalMsg));
  if (st->cfg >= 0 && st->cfg < N_CONFIGS) valvesApply((BenchConfig)st->cfg);   // electrovannes
  if (st->type == 'M') { BPB_BenchState.waitingApproval = true; return; }   // l'operateur doit confirmer

  if (curTable != CALIB) { BPB_BenchState.waitingApproval = true; return; }
  autoT0 = millis();
  autoPhase = 1;
  switch (idx) {
    case 1:                                   // gonflage a 100 mmHg
    case 3:                                   // montee a 150 mmHg puis pente
      if (!BPB_BenchState.abpOk) {            // capteur absent : pas de boucle infinie
        autoFinish("Capteur ABP absent : etape de gonflage/etancheite annulee (brancher le capteur puis relancer la procedure).");
        return;
      }
      motorRun(true);
      break;
    case 4:                                   // pente bras seul : reference immediatement
      autoP0 = BPB_BenchState.pressure;
      break;
    case 5:                                   // zero a l'air libre : purge ouverte par CFG_PURGE
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

    case 5:   // zero : capture apres 3 s de purge ouverte (CFG_PURGE applique)
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
