#pragma once

// RTC BM8563 + NTP.
// L'heure est stockée en UTC dans la RTC : lue au démarrage, réécrite après chaque synchro NTP.

// A appeler au démarrage, avant le WiFi : met l'horloge système à l'heure de la RTC
void timeBegin();
// A appeler une fois le WiFi connecté : lance la synchronisation NTP
void ntpStart();
// A appeler dans loop() : sauvegarde l'heure NTP dans la RTC quand elle arrive
void timeLoop();
