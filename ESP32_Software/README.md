# ESP32_Software — firmware du banc

Firmware du serveur embarqué du banc (ESP32-S3-DevKitC-1-N8R2, Arduino IDE ou PlatformIO).

## Structure — séparation par fonction

| Fichier | Fonction |
| --- | --- |
| `BloodPressureBench.ino` | Point d'entrée : `setup()` / `loop()`, initialisation |
| `config.h` | Brochage, constantes pneumatiques, identifiants WiFi |
| `peripherals.h/.cpp` | **Gestion périphériques** : capteur ABP (I2C), moteur (TMC2209), électrovanne, tâche d'acquisition 100 Hz, métriques d'amplitude et de forme |
| `procedure.h/.cpp` | **Moteur de procédure** : étapes de l'auto-diagnostic et du test, approbation manuelle bloquante |
| `web_server.h/.cpp` | **Serveur web** : API REST (`/api/status`, `/api/start`, `/api/approve`, `/api/value`, `/api/report`), fichiers statiques |
| `www/index.html` | Interface web : statut temps réel, modales d'approbation, formulaire de session |
| `www/report.js` | **Impression / rapport PDF** : assemblage du PDF **côté navigateur** via jsPDF, téléchargement local |

## Dépendances (Bibliothèques Arduino)

- `WiFi`, `WebServer`, `Wire`, `LittleFS` (incluses dans le paquet ESP32)
- `ArduinoJson` (Benoit Blanchon, ≥ 6)

## Provisionnement de `www/`

La page et jsPDF sont stockés dans la partition **LittleFS** de l'ESP32 :

1. Télécharger `jspdf.umd.min.js` (licence MIT, ~110 Ko) et le placer dans `ESP32_Software/www/` ;
2. Ouvrir le dossier `www/` comme **data** dans l'outil "ESP32 Sketch Data Upload" (Arduino IDE) ou `pio run -t uploadfs` (PlatformIO) ;
3. La page fonctionne ensuite **hors Internet** : aucune donnée ne quitte le réseau local.

## Utilisation

1. Alimenter le banc : il crée le réseau WiFi **`Banc-Tensiometre`** (mot de passe `Banc10ans!`) ;
2. S'y connecter depuis un navigateur (téléphone ou PC) — l'interface s'ouvre à la racine ;
3. Suivre la procédure guidée d'auto-diagnostic puis de test ; chaque geste manuel (positions S1-S4, chuck, brassard, seringue de calibration) requiert une **approbation bloquante** ;
4. En fin de session, compléter le formulaire et générer le **rapport PDF**.

## Notes de production

- `procedureRunAuto()` est écrit en pseudo-séquence lisible (boucles + `delay`) : à l'usage définitif, l'implémenter en **machine à états non bloquante** (même modèle que `acquisitionTask`) pour que le serveur web reste réactif pendant les mesures de 60 s ;
- Journaliser chaque session en CSV dans LittleFS en complément du flux série ;
- Le port série (115200 bauds) affiche l'état de démarrage (ABP présent, IP).
