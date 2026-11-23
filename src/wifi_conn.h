#pragma once

// Connexion au WiFi enregistré (ou à celui de config.h), 30 s max.
// Sans WiFi, l'horloge continue sur la RTC.
// onWait est appelée toutes les 50 ms pendant l'attente (pour continuer à afficher l'heure)
void connectWifi(void (*onWait)() = nullptr);
