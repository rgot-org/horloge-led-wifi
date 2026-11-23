#include "ble_prov.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiProv.h>
#include <esp_wifi.h>

volatile ProvState provState = PROV_IDLE;

static unsigned long provStart = 0, successAt = 0;
static String savedSsid, savedPass; // réseau d'avant, restauré si on annule

// Appelé par la tâche des évènements WiFi : on ne fait que changer l'état
static void onProvEvent(arduino_event_t* e) {
	switch (e->event_id) {
	case ARDUINO_EVENT_PROV_START:
		Serial.println("Provisioning BLE démarré");
		break;
	case ARDUINO_EVENT_PROV_CRED_RECV:
		Serial.printf("Identifiants reçus : SSID=%s\n", (const char*)e->event_info.prov_cred_recv.ssid);
		provState = PROV_CONNECTING;
		break;
	case ARDUINO_EVENT_PROV_CRED_FAIL:
		Serial.println(e->event_info.prov_fail_reason == NETWORK_PROV_WIFI_STA_AUTH_ERROR
		               ? "Mot de passe WiFi incorrect" : "Réseau WiFi introuvable");
		provState = PROV_FAILED;
		break;
	case ARDUINO_EVENT_PROV_CRED_SUCCESS:
		Serial.println("Provisioning réussi");
		provState = PROV_SUCCESS;
		break;
	default:
		break;
	}
}

static void provStartBle() {
	// Mémorise le réseau actuel pour pouvoir le restaurer en cas d'annulation
	wifi_config_t conf;
	if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
		savedSsid = (const char*)conf.sta.ssid;
		savedPass = (const char*)conf.sta.password;
	}
	// Repasse en DHCP (l'IP fixe de l'ancien réseau ne conviendrait pas forcément au nouveau)
	WiFi.disconnect();
	WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);

	static bool handlerInstalled = false;
	if (!handlerInstalled) { WiFi.onEvent(onProvEvent); handlerInstalled = true; }

	provState = PROV_WAITING;
	provStart = millis();
	WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BLE,
	                        NETWORK_PROV_SECURITY_1, PROV_POP, PROV_NAME, NULL, NULL,
	                        true /* efface les identifiants enregistrés pour forcer le mode BLE */);
	WiFiProv.printQR(PROV_NAME, PROV_POP, "ble");
}

// Annulation ou délai dépassé : on remet l'ancien réseau et on redémarre
static void provCancel() {
	Serial.println("Provisioning annulé, retour au réseau précédent");
	WiFiProv.endProvision();
	if (savedSsid.length()) WiFi.begin(savedSsid.c_str(), savedPass.c_str()); // réenregistre l'ancien réseau
	delay(500);
	ESP.restart();
}

void provToggle() {
	if (provState == PROV_IDLE) provStartBle();
	else if (provState != PROV_SUCCESS) provCancel();
}

void provLoop() {
	if (provState == PROV_IDLE) return;
	if (provState == PROV_SUCCESS) {
		// Laisse 2 s pour afficher "OK" et pour que l'appli reçoive la confirmation, puis redémarre
		// (au redémarrage, connectWifi() utilise le nouveau réseau enregistré)
		if (!successAt) successAt = millis();
		if (millis() - successAt > 2000) ESP.restart();
		return;
	}
	if (millis() - provStart > PROV_TIMEOUT_MS) provCancel();
}
