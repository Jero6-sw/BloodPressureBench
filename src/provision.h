#ifndef PROVISION_H
#define PROVISION_H

#include <Arduino.h>

// ---------- Provisioning en NVS (Preferences) ----------
// 1er demarrage : aucun reseau configure -> le banc monte son AP
// (Banc-Tensiometre) et l'interface web permet de saisir SSID, mot de passe,
// IP fixe, IP du broker MQTT, CA TLS (PEM) et cle AES-256 -> enregistres en
// NVS. Demarrages suivants : reconnexion STA automatique, l'AP est COUPE
// des qu'un reseau est configure (reactive en secours seulement si le
// reseau est introuvable). Effacement NVS = retour au mode AP.

struct ProvCfg { String ssid, pass, ip, gw, mask;   // reseau
                 String mqttHost, caPem, aesKey; }; // MQTT (broker, TLS, AES)

bool   provHasConfig();               // NVS : un reseau est-il deja enregistre ?
void   provSave(const char* ssid, const char* pass,
                const char* ip, const char* gw, const char* mask);
void   provSaveMqtt(const char* host, const char* ca, const char* aes);
ProvCfg provLoad();                   // lecture NVS (reseau + MQTT)
void   provClear();                   // effacement NVS -> retour au mode AP
void   provApplySta();                // IP fixe + WiFi.begin() : connexion auto
String provSsid();                    // SSID courant (journal serie)

#endif
