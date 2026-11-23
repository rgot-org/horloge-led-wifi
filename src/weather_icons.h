#pragma once
#include <Arduino.h>

// Dessine l'icône météo (boîte 24x24, coin haut-gauche ox,oy)
// icon = code OpenWeatherMap ("01d", "10n", ...), suffixe "n" = nuit
void drawWeatherIcon(int ox, int oy, const String& icon);
