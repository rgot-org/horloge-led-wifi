#pragma once
#include <Arduino.h>

// Mise à jour des réglages à distance par MQTT, via un broker public.
//
// Sujets (topics) :
//   horloge/<id>/cmd     commandes reçues (signées)
//   horloge/<id>/ack     réponse de l'horloge à chaque commande
//   horloge/<id>/status  "online" / "offline" (retenu)
//   horloge/<id>/state   état complet des réglages, JSON (retenu)
//
// Format d'une commande :  <json>.<signature>
//   json      = {"ts":<heure Unix en millisecondes>,"set":{"bright":120,"msg":"Bonjour","theme":"Ocean",...}}
//   signature = HMAC-SHA256(MQTT_SECRET, json) en hexadécimal (64 caractères)
// Clés acceptées dans "set" : les mêmes que la page web (voir settingsApplyKey()).

void mqttBegin();
void mqttLoop();
String mqttDeviceId();
bool mqttConnected();
bool mqttEnabled(); // false tant que MQTT_SECRET n'a pas été changé
void mqttSuspend(); // coupe MQTT le temps d'une requête météo (RAM insuffisante pour 2 connexions TLS)
