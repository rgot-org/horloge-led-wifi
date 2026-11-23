#pragma once
#include <stdint.h>

// Réglage du WiFi par Bluetooth (BLE) avec l'appli Espressif « ESP BLE Provisioning ».
// Lancé par un appui sur le bouton de la carte ; un 2e appui annule.

enum ProvState : uint8_t {
	PROV_IDLE,       // pas de réglage en cours
	PROV_WAITING,    // en attente du téléphone
	PROV_CONNECTING, // identifiants reçus, connexion en cours
	PROV_SUCCESS,    // connecté : la carte va redémarrer
	PROV_FAILED      // mauvais mot de passe / réseau introuvable (on peut réessayer depuis l'appli)
};

extern volatile ProvState provState;

void provToggle(); // démarre le réglage, ou l'annule s'il est en cours (appui bouton)
void provLoop();   // à appeler dans loop() : gère la fin (redémarrage) et le délai max
