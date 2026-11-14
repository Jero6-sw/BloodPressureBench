#ifndef MQTT_LINK_H
#define MQTT_LINK_H

// ---------- MQTT TLS : stockage des mesures d'etalonnage + tension ----------
// - banc/tensiometre/calib    : mesures d'etalonnage (JSON, transport TLS)
// - banc/tensiometre/mesures : mesures de tension CHIFFREES AES-256-GCM
//   (TLS protege le transport, l'AES protege le message de bout en bout).
// Broker, CA TLS et cle AES lus depuis la NVS (interface web) ; config.h =
// valeurs de repli. MQTT inactif si aucun reseau n'est configure en NVS.

void mqttSetup();                   // CA epingle + serveur broker (appelee par setupServer)
void mqttLoop();                    // keepalive + reconnexion (a appeler dans loop())
bool mqttPublishCalib();            // mesures d'etalonnage de la session
bool mqttPublishMeasures();         // mesures de tension chiffrees + patient

#endif
