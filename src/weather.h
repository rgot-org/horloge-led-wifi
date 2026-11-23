#pragma once
#include <Arduino.h>

struct WeatherData {
	float  temperature = 0; // °C
	float  humidity    = 0; // %
	float  windspeed   = 0; // m/s
	String icon;            // code OpenWeatherMap : "01d", "10n", ...
	String description;
	String cityName;        // nom de la ville renvoyé par OpenWeatherMap
	time_t sunrise = 0;     // heure Unix (UTC)
	time_t sunset  = 0;
};

extern WeatherData weather;
extern bool        weatherOk;       // au moins une météo reçue
extern uint32_t    weatherVersion;  // incrémenté à chaque nouvelle météo (pour redessiner)
extern int         weatherHttpCode; // dernier code HTTP (200 = OK, 404 = ville introuvable)
extern volatile bool weatherRefresh; // demande de mise à jour immédiate (changement de ville)

// Interroge OpenWeatherMap (bloquant 1 à 2 s). Retourne true si la météo a été mise à jour.
bool fetchWeather();
