#include "valves.h"
#include "config.h"
#include "peripherals.h"

// Table broches/type : true = NO (energisee = fermee), false = NC (energisee = ouverte)
struct ValveDef { int pin; bool no; const char* name; };
static const ValveDef VDEF[7] = {
  {PIN_V0, true,  "V0 S1-serie"  },
  {PIN_V1, true,  "V1 PULSATION" },
  {PIN_V2, false, "V2 POMPE"     },
  {PIN_V3, false, "V3 manometre" },
  {PIN_V4, false, "V4 isolement bras" },
  {PIN_V5, true,  "V5 isolement clapets" },
  {PIN_V6, false, "V6 purge"     },
};
static bool energized[7] = {false, false, false, false, false, false, false};

static void drive(int v, bool open) {
  // NO : ouvert = de-energise ; NC : ouvert = energise
  energized[v] = VDEF[v].no ? !open : open;
  digitalWrite(VDEF[v].pin, energized[v] ? HIGH : LOW);
}

void valvesInit() {
  for (int v = 0; v < 7; v++) {
    pinMode(VDEF[v].pin, OUTPUT);
    digitalWrite(VDEF[v].pin, LOW);       // de-energise partout
    energized[v] = false;
  }
  valvesApply(CFG_STORAGE);               // etat de securite au demarrage
}

const char* valvesConfigName(BenchConfig c) {
  static const char* names[N_CONFIGS] = {
    "Gonflage (pompe)", "Pulsation/mesure", "Banc seul", "Bras seul",
    "Lectures croisees", "Boyle", "Stockage", "Purge"
  };
  return names[c];
}

void valvesApply(BenchConfig cfg) {
  switch (cfg) {
    case CFG_INFLATE:   // S1 2 / S2 2 / S3 1 / S4 1
      drive(0, true); drive(1, false); drive(2, true);
      drive(3, false); drive(4, true); drive(5, true); drive(6, false);
      break;
    case CFG_MEASURE:   // S1 1 / S2 2 / S3 1 / S4 2
      drive(0, true); drive(1, true); drive(2, false);
      drive(3, false); drive(4, true); drive(5, false); drive(6, false);
      break;
    case CFG_BENCH_ONLY: // S1 1 / S2 2 / S3 2 / S4 2
      drive(0, true); drive(1, true); drive(2, false);
      drive(3, false); drive(4, false); drive(5, false); drive(6, false);
      break;
    case CFG_ARM_ONLY:  // S1 3 / S2 2 / S3 1 / S4 2
      drive(0, false); drive(1, false); drive(2, false);
      drive(3, false); drive(4, true); drive(5, false); drive(6, false);
      break;
    case CFG_GAUGE:     // S2 1 / S3 1
      drive(3, true); drive(4, true);
      break;
    case CFG_BOYLE:     // S1 3 / S2 1 / S3 1 / S4 2
      drive(0, false); drive(1, false); drive(2, false);
      drive(3, true); drive(4, true); drive(5, false); drive(6, false);
      break;
    case CFG_PURGE:     // degonflage : vanne atmo ouverte, bras raccorde
      drive(0, true); drive(1, false); drive(2, false);
      drive(3, false); drive(4, true); drive(5, false); drive(6, true);
      break;
    case CFG_STORAGE:   // tout ferme/isole
    default:
      for (int v = 0; v < 7; v++) drive(v, false);
      break;
  }
}

// ---------- Auto-test d'isolement d'une vanne ----------
// Principe : configurer le circuit pour que la vanne v separe un segment
// pressurise du reste (pente cote amont). Une vanne NO fermee doit tenir
// sans consommation ; une vanne NC fermee est de-energisee.
// NOTE : la mise en pression du segment s'appuie sur la pompe (V2) ; le
// pilotage precis par segment est a ajuster selon le cablage reel.
bool valveLeakTest(int v, float p0_mmHg, float* slopeOut) {
  // 1. fermer strictement la vanne testee
  drive(v, false);
  // 2. pente de fuite sur 60 s au capteur ABP
  float pStart = S.pressure;
  delay(60000);
  float slope = S.pressure - pStart;      // mmHg/min
  if (slopeOut) *slopeOut = slope;
  return slope < LEAK_SLOPE_MAX;          // seuil : < 1 mmHg/min
}

bool valveIsEnergized(int v) { return (v >= 0 && v < 7) ? energized[v] : false; }
