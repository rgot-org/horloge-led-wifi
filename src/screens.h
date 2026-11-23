#pragma once
#include <time.h>

// Les 3 zones de l'écran 64x96 (une par panneau)
void drawClock(struct tm* tm, bool colonOn); // panneau du haut   : HH:MM + barres des secondes
void drawDay(struct tm* tm, int oy);         // panneau du milieu : jour, date, température, humidité
void drawBottom(int oy);                     // panneau du bas    : météo ou message défilant
                                             //   (ou réglage Bluetooth / adresse web)
void drawWeather(int oy);                    // icône, vent, lever/coucher du soleil

// Numéro d'image de l'animation du panneau du bas (défilement), -1 si rien ne bouge.
// Quand il change, il faut redessiner.
int bottomAnimFrame();
