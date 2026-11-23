#include "weather.h"
#include "config.h"
#include "settings.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

WeatherData weather;
bool        weatherOk = false;
uint32_t    weatherVersion = 0;
int         weatherHttpCode = 0;
volatile bool weatherRefresh = false;

static bool decodeWeather(Stream& json) {
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, json);
	if (error) {
		Serial.printf("deserializeJson() failed: %s\n", error.c_str());
		return false;
	}
	weather.temperature = doc["main"]["temp"].as<float>();
	weather.humidity    = doc["main"]["humidity"].as<float>();
	weather.windspeed   = doc["wind"]["speed"].as<float>();
	weather.icon        = doc["weather"][0]["icon"].as<String>();
	weather.description = doc["weather"][0]["description"].as<String>();
	weather.cityName    = doc["name"].as<String>();
	weather.sunrise     = doc["sys"]["sunrise"].as<long>();
	weather.sunset      = doc["sys"]["sunset"].as<long>();
	weatherOk = true;
	weatherVersion++;
	Serial.printf("%s : T=%.1f H=%.0f icon=%s\n", weather.cityName.c_str(), weather.temperature, weather.humidity, weather.icon.c_str());
	return true;
}

// Encode la ville pour l'URL (espaces, accents...)
static String urlEncode(const String& s) {
	String out;
	for (unsigned char c : s) {
		if (isalnum(c) || c == ',' || c == '-' || c == '.' || c == '_') out += (char)c;
		else { char b[4]; snprintf(b, sizeof(b), "%%%02X", c); out += b; }
	}
	return out;
}

bool fetchWeather() {
	WiFiClientSecure client;
	client.setInsecure();
	HTTPClient https;
	String uri = "/data/2.5/weather?q=" + urlEncode(settings.city) +
	             "&APPID=" OWM_API_KEY "&mode=json&units=metric&lang=" OWM_LANG;
	int httpCode = -1;
	for (int r = 0; r < 5 && httpCode <= 0; r++) {
		https.begin(client, "api.openweathermap.org", 443, uri);
		httpCode = https.GET();
		if (httpCode <= 0) {
			Serial.printf("fetchWeather: échec, code=%d, nouvel essai...\n", httpCode);
			https.end();
			delay(1000);
		}
	}
	weatherHttpCode = httpCode;
	if (httpCode == 404) Serial.printf("Ville introuvable : %s\n", settings.city.c_str());
	bool ok = (httpCode == HTTP_CODE_OK) && decodeWeather(https.getStream());
	https.end();
	return ok;
}
