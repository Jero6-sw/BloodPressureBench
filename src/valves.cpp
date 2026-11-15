#include "valves.h"
#include "config.h"
#include "peripherals.h"

// Table broches/type : true = NO (energisee = fermee), false = NC (energisee = ouverte)
// V0/V4 : retirees du banc (v1.8), pilotees en etat transparent si encore cablees.
struct ValveDef { int pin; bool no; const char* name; };
static const ValveDef VDEF[7] = {
  {PIN_V0, true,  "V0 (retiree)"  },
  {PIN_V1, true,  "V1 PULSATION" },
  {PIN_V2, false, "V2 POMPE"     },
  {PIN_V3, false, "V3 manometre" },
  {PIN_V4, false, "V4 (retiree)" },
  {PIN_V5, true,  "V5 clapets" },
  {PIN_V6, false, "V6 purge"     },
};
static bool energized[7] = {false, false, false, false, false, false, false};
static bool openState [7] = {false, false, false, false, false, false, false};
static int  curCfg = -1;                    // derniere configuration appliquee

static void drive(int v, bool open) {
  // NO : ouvert = de-energise ; NC : ouvert = energise
  energized[v] = VDEF[v].no ? !open : open;
  openState[v]  = open;
  digitalWrite(VDEF[v].pin, energized[v] ? HIGH : LOW);
}

void valveDrive(int v, bool open) { if (v >= 0 && v < 7) drive(v, open); }
bool valveIsOpen(int v)          { return (v >= 0 && v < 7) && openState[v]; }

void valvesInit() {
  for (int v = 0; v < 7; v++) {
    pinMode(VDEF[v].pin, OUTPUT);
    digitalWrite(VDEF[v].pin, LOW);       // de-energise partout
    energized[v] = false; openState[v] = false;
  }
  valvesApply(CFG_STORAGE);               // etat de securite au demarrage
}

static const char* cfgNames[N_CONFIGS] = {
  "Gonflage (pompe)", "Pulsation/mesure", "Lectures croisees", "Boyle (port S2)", "Purge atmo", "Stockage"
};

const char* valvesConfigName(BenchConfig cfg) { return cfgNames[cfg]; }
const char* valvesCurrentName() { return (curCfg >= 0 && curCfg < N_CONFIGS) ? cfgNames[curCfg] : "Manuel"; }

void valvesApply(BenchConfig cfg) {
  curCfg = cfg;
  BPB_BenchState.valveOpen = false;       // remis a true par CFG_PURGE
  switch (cfg) {
    case CFG_PUMP:     // montee en pression : uniquement via la branche clapets
      drive(1, false); drive(2, true); drive(3, false); drive(5, true); drive(6, false);
      break;
    case CFG_MEASURE:  // pulsation directe seringue <-> M
      drive(1, true); drive(2, false); drive(3, false); drive(5, false); drive(6, false);
      break;
    case CFG_GAUGE:    // lectures croisees : manometre connecte
    case CFG_BOYLE:    // Boyle : seringue 10 mL sur le port S2 (cote manometre)
      drive(1, false); drive(2, false); drive(3, true); drive(5, false); drive(6, false);
      break;
    case CFG_PURGE:    // degonflage a 0 mmHg
      drive(1, false); drive(2, false); drive(3, false); drive(5, false); drive(6, true);
      BPB_BenchState.valveOpen = true;
      break;
    case CFG_STORAGE:  // tout ferme
    default:
      drive(1, false); drive(2, false); drive(3, false); drive(5, false); drive(6, false);
      break;
  }
  // V0/V4 retirees : etat transparent si le cablage existe encore
  drive(0, false);   // V0 fermee (sans effet si absente)
  drive(4, true);    // V4 ouverte : le bras reste toujours dans le circuit
}

// ---------- Auto-test d'isolement d'une vanne ----------
bool valveLeakTest(int v, float p0_mmHg, float* slopeOut) {
  drive(v, false);
  float pStart = BPB_BenchState.pressure;
  delay(60000);
  float slope = BPB_BenchState.pressure - pStart;      // mmHg/min
  if (slopeOut) *slopeOut = slope;
  return slope < LEAK_SLOPE_MAX;          // seuil : < 1 mmHg/min
}

bool valveIsEnergized(int v) { return (v >= 0 && v < 7) ? energized[v] : false; }
