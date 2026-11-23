#include "wifi_conn.h"
#include "config.h"
#include <WiFi.h>
#include <esp_wifi.h>

void connectWifi(void (*onWait)()) {
	WiFi.disconnect();
	WiFi.mode(WIFI_STA);

	// Réseau enregistré (réglé par Bluetooth ou dernière connexion), sinon celui de config.h
	wifi_config_t conf;
	if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK && conf.sta.ssid[0] != 0) {
		Serial.printf("WiFi enregistré : %s\n", (const char*)conf.sta.ssid);
		WiFi.begin();
	} else {
		Serial.printf("WiFi par défaut : %s\n", DEFAULT_SSID);
		WiFi.begin(DEFAULT_SSID, PASS);
	}

	// 30 s max, en continuant d'afficher l'heure (RTC) pendant l'attente
	for (int i = 0; i < 600 && WiFi.status() != WL_CONNECTED; i++) {
		if (onWait) onWait();
		delay(50);
		if (i % 10 == 0) Serial.print(".");
	}
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("\nWiFi indisponible, on continue avec l'heure de la RTC");
		return;
	}
	// Le DHCP a fourni l'IP/passerelle/masque : on les fige en "statique"
	// pour pouvoir forcer un DNS public fiable (le DNS du routeur ne résout pas les noms externes)
	WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), IPAddress(1, 1, 1, 1), IPAddress(8, 8, 8, 8));
	// Pas de mise en veille de la radio : sinon les connexions entrantes (page web) se perdent
	WiFi.setSleep(false);
	Serial.printf("\nWiFi Connected.\nIP Address: %s\nDNS: %s\n",
	              WiFi.localIP().toString().c_str(), WiFi.dnsIP().toString().c_str());
}
