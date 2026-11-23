/*
  Horloge WiFi - carte HD-WF2 + 3 panneaux HUB75 64x32 empilés (écran 64x96)
    panneau du haut   : heure HH:MM, ":" clignotants, barres des secondes
    panneau du milieu : jour, date, température, humidité
    panneau du bas    : icône météo, vent, lever/coucher du soleil

  Réglages (WiFi, ville, fuseau, broches, panneaux) : config.h

  configuration
  UsbMode : Hardware CDC and JTAG
  USB CDC On Boot : Disabled
  Upload Mode : USB-OTG (TinyUSB)
  Flash Mode : DIO 80MHz (preferences ne fonctionne pas si autre mode)
*/
#include <Arduino.h>
#include <WiFi.h>
#include <EasyButton.h>
#include "config.h"
#include "display.h"
#include "screens.h"
#include "weather.h"
#include "wifi_conn.h"
#include "timekeeping.h"
#include "ble_prov.h"
#include "settings.h"
#include "webui.h"
#include "mqtt_remote.h"

EasyButton btn(WF2_BUTTON_TEST);

// Redessine l'écran si quelque chose a changé (seconde, ":" clignotants, météo, état WiFi)
void render() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	// ":" clignotants à 1 Hz (500 ms allumé / 500 ms éteint), synchronisés sur la seconde
	bool colonOn = tv.tv_usec < 500000;
	bool wifiOk = WiFi.status() == WL_CONNECTED;

	static time_t lastSec = 0;
	static bool lastColon = false, lastWifi = false;
	static ProvState lastProv = PROV_IDLE;
	static int lastAp = 0;
	int ap = webApActive() ? 1 + (WiFi.softAPgetStationNum() > 0) : 0; // Wi-Fi de réglage : fermé / attente / connecté
	static uint32_t lastWeatherVersion = 0, lastSettingsVersion = 0;
	static int lastFrame = -1;
	int frame = bottomAnimFrame(); // défilement du message
	if (tv.tv_sec == lastSec && colonOn == lastColon && wifiOk == lastWifi && provState == lastProv && ap == lastAp &&
	    weatherVersion == lastWeatherVersion && settingsVersion == lastSettingsVersion && frame == lastFrame) return;
	lastFrame = frame;
	lastSec = tv.tv_sec;
	lastColon = colonOn;
	lastWifi = wifiOk;
	lastProv = provState;
	lastAp = ap;
	lastWeatherVersion = weatherVersion;
	lastSettingsVersion = settingsVersion;

	struct tm tm;
	localtime_r(&tv.tv_sec, &tm);

	matrix->fillScreen(0);
	drawClock(&tm, colonOn);
	drawDay(&tm, ZONE_H);
	drawBottom(2 * ZONE_H);
	matrix->flipDMABuffer();
}

// Minutes depuis minuit (heure locale) d'une heure Unix
static int minutesOfDay(time_t t) {
	struct tm lt;
	localtime_r(&t, &lt);
	return lt.tm_hour * 60 + lt.tm_min;
}

// Luminosité de jour ou de nuit (nuit = entre le coucher et le lever du soleil,
// ou 21h-7h tant que la météo n'a pas donné les heures du soleil)
void updateBrightness() {
	time_t now = time(NULL);
	int m = minutesOfDay(now);
	int rise = 7 * 60, set = 21 * 60;
	if (weatherOk && weather.sunrise && weather.sunset) {
		rise = minutesOfDay(weather.sunrise);
		set  = minutesOfDay(weather.sunset);
	}
	nightActive = settings.nightOn && (m >= set || m < rise);
	uint8_t target = nightActive ? settings.nightBright : settings.brightness;
	static int applied = -1;
	if (target != applied) {
		displaySetBrightness(target);
		applied = target;
	}
}

// Appui long : réglage du Wi-Fi par Bluetooth (ferme d'abord le Wi-Fi de réglage s'il est ouvert)
static void onLongPress() {
	if (webApActive()) webApToggle();
	provToggle();
}

void setup() {
	Serial.begin(115200);
	delay(1000); // attendre que le port série soit prêt pour les messages de debug
	settingsLoad();        // couleurs et luminosité mémorisées
	displaySetup();
	timeBegin();          // heure de la RTC, avant même le WiFi
	connectWifi(render);  // l'heure reste affichée pendant la connexion
	ntpStart();
	webBegin();           // prépare l'interface de réglage (ouverte par le bouton)
	mqttBegin();          // mise à jour à distance par MQTT (messages signés)
	btn.begin();
	btn.onPressed(webApToggle);                       // appui court : Wi-Fi privé de réglage (ouvre / ferme)
	btn.onPressedFor(BTN_LONG_PRESS_MS, onLongPress); // appui long : réglage du Wi-Fi par Bluetooth
	Serial.printf("Mémoire libre : %u octets (plus grand bloc : %u)\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

void loop() {
	static time_t lastWeather = 0;

	btn.read();
	timeLoop();           // sauvegarde l'heure NTP dans la RTC quand elle arrive
	provLoop();           // réglage WiFi par Bluetooth en cours
	webLoop();            // interface web + sauvegarde des réglages
	mqttLoop();

	updateBrightness();   // jour / nuit, ou réglage modifié depuis la page web

	// Mise à jour météo (périodique, ou tout de suite après un changement de ville)
	time_t now = time(NULL);
	if (weatherRefresh) {
		weatherRefresh = false;
		lastWeather = 0;
	}
	if (provState == PROV_IDLE && now > lastWeather + WEATHER_PERIOD_S) { // pas pendant le réglage Bluetooth
		if (WiFi.status() != WL_CONNECTED) {
			// Pas de WiFi : l'horloge continue sur la RTC, on retente dans 1 min
			WiFi.reconnect();
			lastWeather = now - WEATHER_PERIOD_S + 60;
		} else {
			mqttSuspend();     // une seule connexion TLS à la fois (RAM limitée)
			fetchWeather();
			lastWeather = time(NULL);
		}
	}

	render();
}
