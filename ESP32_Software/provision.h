#ifndef PROVISION_H
#define PROVISION_H

#include <Arduino.h>

// ---------- Provisioning reseau en NVS (Preferences) ----------
// 1er demarrage : aucun reseau configure -> le banc reste en mode AP
// (Banc-Tensiometre) et l'interface web permet de saisir SSID, mot de passe
// et IP fixe -> enregistres en NVS. Demarrages suivants : reconnexion STA
// automatique des que le reseau sauvegarde est disponible (AP maintenu
// en secours pour l'interface). Effacement NVS = retour au mode AP.

struct ProvCfg { String ssid, pass, ip, gw, mask; };

bool   provHasConfig();               // NVS : un reseau est-il deja enregistre ?
void   provSave(const char* ssid, const char* pass,
                const char* ip, const char* gw, const char* mask);
ProvCfg provLoad();                   // lecture NVS
void   provClear();                   // effacement NVS -> retour au mode AP
void   provApplySta();                // IP fixe + WiFi.begin() : connexion auto
String provSsid();                    // SSID courant (journal serie)

#endif
