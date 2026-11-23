#pragma once

// Interface web de réglage, accessible UNIQUEMENT par un Wi-Fi privé ouvert par l'horloge.
// Rien n'est exposé sur le réseau public auquel l'horloge est connectée.
//   - appui court sur le bouton : ouvre (ou ferme) le Wi-Fi de réglage
//   - mot de passe WPA2 aléatoire, nouveau à chaque ouverture, affiché sur le panneau du bas
//   - fermeture automatique après AP_IDLE_TIMEOUT_MS sans activité

void webBegin();               // à appeler une fois dans setup() (prépare le serveur, ne l'ouvre pas)
void webLoop();                // à appeler dans loop()
void webApToggle();            // ouvre / ferme le Wi-Fi de réglage
bool webApActive();
const char* webApSsid();
const char* webApPassword();

#include <ArduinoJson.h>
// Etat complet (réglages, thèmes, météo, MQTT) au format JSON : page web et publication MQTT
void buildSettingsJson(JsonDocument& doc);
