#ifndef MQTT_LINK_H
#define MQTT_LINK_H

// ---------- MQTT TLS : stockage des mesures d'etalonnage + tension ----------
// - Banc/tensiometre/calib   : mesures d'etalonnage (JSON, transport TLS)
// - Banc/tensiometre/mesures : mesures de tension CHIFFREES AES-256-GCM
//   (TLS protege le transport, l'AES protege le message de bout en bout).
// Liste des patients predefinie dans config.h (PATIENT_LIST).
// Silencieux si MQTT_STA_SSID == "CHANGE" (banc autonome, publication differee).

void mqttSetup();                   // connexion STA + broker (appelee par setupServer)
void mqttLoop();                    // keepalive + reconnexion (a appeler dans loop())
bool mqttPublishCalib();            // mesures d'etalonnage de la session
bool mqttPublishMeasures();         // mesures de tension chiffrees + patient

#endif
