#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>

// Instance du serveur web propre au banc. Prefixe BPB_ : ne pas confondre
// avec le type generique WebServer de la librairie ESP32.
// ATTENTION : c'est une VARIABLE, pas une fonction -> pas de parentheses.
extern WebServer BPB_WebServer;

void setupServer();   // endpoints REST + fichiers statiques (LittleFS /www)

#endif
