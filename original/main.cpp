/*
configuration
UsbMode : Hardware CDC and JTAG
USB CDC On Boot : Disabled
Upload Mode : USB-OTG (TinyUSB)
Flash Mode : DIO 80MHz (preferences ne fonctionne pas si autre mode)
*/
//#include "certificat.h"
#include <EasyButton.h>
#include <HTTPClient.h>
#include <ArduinoJson.hpp>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Adafruit_GFX.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h> // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/issues/433
#include <Fonts/FreeSansBold9pt7b.h>
#include <Preferences.h>
#include "IPGeolocation.h"

#define max_readings 24
#define DEFAULT_SSID "MonReseauWiFi"
#define PASS "MonMotDePasseWiFi"
// #define DEFAULT_SSID "AutreReseau"
// #define PASS "AutreMotDePasse"
#define PORT 443
/*********************  HD-WF2 ************************/
#define VERSION HD-WF2

#if VERSION == 11
/***** version 1.1 ********/
#define WF2_X1_R1_PIN 8
#define WF2_X1_R2_PIN 10
#define WF2_X1_G1_PIN 1
#define WF2_X1_G2_PIN 2
#define WF2_X1_B1_PIN 9
#define WF2_X1_B2_PIN 11
#define WF2_X1_E_PIN 3

#define WF2_A_PIN 12
#define WF2_B_PIN 4
#define WF2_C_PIN 14
#define WF2_D_PIN 5
#define WF2_OE_PIN 42
#define WF2_CLK_PIN 46
#define WF2_LAT_PIN  6
#elif VERSION == 15
/****** version 1.5 IAN *******/
#define WF2_X1_R1_PIN 12
#define WF2_X1_R2_PIN 46
#define WF2_X1_G1_PIN 11
#define WF2_X1_G2_PIN 10
#define WF2_X1_B1_PIN 14
#define WF2_X1_B2_PIN 9
#define WF2_X1_E_PIN 8

#define WF2_A_PIN 7
#define WF2_B_PIN 6
#define WF2_C_PIN 5
#define WF2_D_PIN 4
#define WF2_OE_PIN 1
#define WF2_CLK_PIN 3
#define WF2_LAT_PIN  2

#elif VERSION == HD-WF2
#define WF2_X1_R1_PIN 2
#define WF2_X1_R2_PIN 3
#define WF2_X1_G1_PIN 6
#define WF2_X1_G2_PIN 7
#define WF2_X1_B1_PIN 10
#define WF2_X1_B2_PIN 11
#define WF2_X1_E_PIN 21

#define WF2_A_PIN 39
#define WF2_B_PIN 38
#define WF2_C_PIN 37
#define WF2_D_PIN 36
#define WF2_OE_PIN 35
#define WF2_CLK_PIN 34
#define WF2_LAT_PIN 33
#endif
#define WF2_BUTTON_TEST     17  // Test key button on PCB, 1=normal, 0=pressed
#define WF2_LED_RUN_PIN     40  // Status LED on PCB
#define WF2_BM8563_I2C_SDA  41  // RTC BM8563 I2C port
#define WF2_BM8563_I2C_SCL  42

#define PANEL_RES_X 64     // Number of pixels wide of each INDIVIDUAL panel module.
#define PANEL_RES_Y 32     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another
HUB75_I2S_CFG::i2s_pins _pins_x1 = { WF2_X1_R1_PIN, WF2_X1_G1_PIN, WF2_X1_B1_PIN, WF2_X1_R2_PIN, WF2_X1_G2_PIN, WF2_X1_B2_PIN, WF2_A_PIN, WF2_B_PIN, WF2_C_PIN, WF2_D_PIN, WF2_X1_E_PIN, WF2_LAT_PIN, WF2_OE_PIN, WF2_CLK_PIN };
//HUB75_I2S_CFG::i2s_pins _pins_x2 = { WF2_X2_R1_PIN, WF2_X2_G1_PIN, WF2_X2_B1_PIN, WF2_X2_R2_PIN, WF2_X2_G2_PIN, WF2_X2_B2_PIN, WF2_A_PIN, WF2_B_PIN, WF2_C_PIN, WF2_D_PIN, WF2_X2_E_PIN, WF2_LAT_PIN, WF2_OE_PIN, WF2_CLK_PIN };


// -------------------------------------
// -------   Matrix Config   ------
// -------------------------------------
MatrixPanel_I2S_DMA *matrix = nullptr;
Preferences preferences;
EasyButton btn(WF2_BUTTON_TEST);
String geokey = "VOTRE_CLE_IPREGISTRY";
String apikey = "VOTRE_CLE_OPENWEATHERMAP";                      // See: https://openweathermap.org/  // It's free to get an API key, but don't take more than 60 readings/minute!
const char server[] = "api.openweathermap.org";
//http://api.openweathermap.org/data/2.5/weather?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=1   // Example API call for weather data
//http://api.openweathermap.org/data/2.5/forecast?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=40 // Example API call for forecast data
//Set your location according to OWM locations

String City = "", Country = "";
float lat = 0, lon = 0;
														   // https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes
String Language = "FR";                            // NOTE: Only the weather description is translated by OWM
														   // Examples: Arabic (AR) Czech (CZ) English (EN) Greek (EL) Persian(Farsi) (FA) Galician (GL) Hungarian (HU) Japanese (JA)
														   // Korean (KR) Latvian (LA) Lithuanian (LT) Macedonian (MK) Slovak (SK) Slovenian (SL) Vietnamese (VI)
String Hemisphere = "north";                         // or "south"
String Units = "M";                             // Use 'M' for Metric or I for Imperial
const char* Timezone = "CET-1CEST,M3.5.0,M10.5.0/3";  // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
														   // See below for examples
const char* ntpServer = "0.uk.pool.ntp.org";             // Or, choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
														   // then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
														   // EU "0.europe.pool.ntp.org"
														   // US "0.north-america.pool.ntp.org"
														   // See: https://www.ntppool.org/en/
int   gmtOffset_sec = 0;    // UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800
int  daylightOffset_sec = 3600; // In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset
typedef struct { // For current Day and Day 1, 2, 3, etc
	String   Dt;
	String   Period;
	String   Icon;
	String   Trend;
	String   Main0;
	String   Forecast0;
	String   Forecast1;
	String   Forecast2;
	String   Description;
	String   Time;
	String   Country;
	float    lat;
	float    lon;
	float    Temperature;
	float    Humidity;
	float    High;
	float    Low;
	float    Winddir;
	float    Windspeed;
	float    Rainfall;
	float    Snowfall;
	float    Pressure;
	int      Cloudcover;
	int      Visibility;
	int      Sunrise;
	int      Sunset;
	int      Timezone;
} Forecast_record_type;

Forecast_record_type  WxConditions[1];
#define TZ (1*60*60) /*JST*/

//******************

	bool DecodeWeather(NetworkClient & json) {
	Serial.print(F("\nCreating object...and "));
	// allocate the JsonDocument
	DynamicJsonDocument doc(35 * 1024);
	// Deserialize the JSON document
	DeserializationError error = deserializeJson(doc, json);
	// Test if parsing succeeds.
	if (error) {
		Serial.print(F("deserializeJson() failed: "));
		return false;
	}
	// convert it to a JsonObject
	JsonObject root = doc.as<JsonObject>();
		WxConditions[0].Temperature = root["main"]["temp"].as<float>();              //Serial.println("Temp: "+String(WxConditions[0].Temperature));
	return true;
}
bool obtain_wx_data() {
	const String units = (Units == "M" ? "metric" : "imperial");
	WiFiClientSecure client;
	client.setInsecure();
	HTTPClient https;
	//String uri = "/data/2.5/weather?q=" + City + "," + Country + "&APPID=" + apikey + "&mode=json&units=" + units + "&lang=" + Language;
	String uri="/data/2.5/weather?q=PARIS,FR&APPID=VOTRE_CLE_OPENWEATHERMAP&mode=json&units=metric&lang=FR";
	uri += "&cnt=" + String(max_readings);
	Serial.println(uri);
	int httpCode = -1;
	int r = 0; //retry counter
	while (httpCode <= 0 && r < 5) {
		https.begin(client, "api.openweathermap.org", 443, uri);
		httpCode = https.GET();
		if (httpCode <= 0) {
			Serial.printf("obtain_wx_data request failed, code=%d, retrying...\n", httpCode);
			https.end();
			delay(1000);
		}
		r++;
	}
	if (httpCode == HTTP_CODE_OK) {
		if (!DecodeWeather(https.getStream())) return false;
		//client.stop();
		https.end();
		return true;
	}
	https.end();
	return true;
}
void getLocation() {
	if (lat == 0)
	{
		// IPGeolocation location(geokey, "IPREGISTRY");
		// IPGeo IPG;
		// location.updateStatus(&IPG);
		// City = IPG.city;
		// Serial.println(City);
		// Country = IPG.country;
		lat = 48.859077;//IPG.latitude;
		lon = 2.293486;//IPG.longitude;
		Serial.printf("lat=%f lon=%f\n", lat, lon);
		preferences.begin("meteo-app", false);
		preferences.putString("City", City);
		preferences.putString("Country", Country);
		preferences.putFloat("lat", lat);
		preferences.putFloat("lon", lon);
		// interroger openweathermap pour connaitre la durée du jour la plus courte/ la plus longue.
		preferences.end();
	}
}
void  smartConfig() {
	matrix->fillScreen(0);
	matrix->setTextColor(matrix->color565(255, 0, 0));
	matrix->setFont();
	matrix->setCursor(0, 0);
	matrix->println("  Waiting");
	matrix->println("    for");
	matrix->println(" EspTouch");
	matrix->flipDMABuffer();
	WiFi.beginSmartConfig();
    // pour ESP32 S3 ne fonctionne que si le mode de programmation est OPI
	// voir https://github.com/espressif/arduino-esp32/issues/8429
	//Wait for SmartConfig packet from mobile
	Serial.println("Waiting for SmartConfig.");
	for (size_t i = 0; i < 200; i++) { // wait 1.5min
		if (WiFi.smartConfigDone())
		{
			Serial.println("");
			Serial.println("SmartConfig received.");

			//Wait for WiFi to connect to AP
			Serial.println("Waiting for WiFi");
			while (WiFi.status() != WL_CONNECTED) {
				delay(500);
				Serial.print(".");
			}
			preferences.begin("wifi-config", false);
			preferences.putString("SSID", WiFi.SSID());
			preferences.putString("PassPhrase", WiFi.psk());
			// WiFi.setStr
			Serial.println("\nAP : " + preferences.getString("SSID"));
			Serial.println("PASS : " + preferences.getString("PassPhrase"));
			preferences.end();
			break;
		}
		else
		{
			delay(500);
			Serial.print(".");
		}
	}
}

void connectWifi() {
	matrix->fillScreen(0);
	matrix->setTextColor(matrix->color565(255, 0, 0));
	matrix->setFont();
	matrix->setCursor(0, 16);
	matrix->println("Connecting");
	matrix->println("Wifi");
	WiFi.disconnect();
	WiFi.mode(WIFI_STA);
	preferences.begin("wifi-config", true);
	String SSID = DEFAULT_SSID;// preferences.getString("SSID", DEFAULT_SSID);
	String PassPhrase = PASS;//preferences.getString("PassPhrase", PASS);
	preferences.end();
	if (SSID.length() > 0)
	{
		WiFi.begin(SSID.c_str(), PassPhrase.c_str());
		while (WiFi.status() != WL_CONNECTED) {
			delay(500);
			Serial.print(".");
		}

		// Le DHCP a fourni l'IP/passerelle/masque : on les fige en "statique"
		// pour pouvoir forcer un DNS public fiable (le DNS du routeur ne résout pas les noms externes)
		WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), IPAddress(1, 1, 1, 1), IPAddress(8, 8, 8, 8));

		// for (size_t i = 0; i < 40; i++)
		// {
		// 	if (WiFi.status() == WL_CONNECTED) break;
		// 	else {
		// 		delay(500);
		// 		Serial.print(".");
		// 	}
		// }
		// if (WiFi.status() != WL_CONNECTED) smartConfig();
	}
	else
	{
		smartConfig();
	}
	//getLocation();
	Serial.println("WiFi Connected.");
	Serial.print("IP Address: ");
	Serial.println(WiFi.localIP());
	Serial.print("DNS: ");
	Serial.println(WiFi.dnsIP());
}


void displaySetup() {
	HUB75_I2S_CFG mxconfig(
		PANEL_RES_X,   // module width
		PANEL_RES_Y,   // module height
		PANEL_CHAIN,   // Chain length
		_pins_x1       // pin mapping for port X1
	);
//	mxconfig.clkphase = false;
	mxconfig.double_buff = true;
	matrix = new MatrixPanel_I2S_DMA(mxconfig);
	matrix->begin();
	matrix->setBrightness8(63); //0-255
}

void setup() {
	Serial.begin(115200);
	displaySetup();
	delay(100);
	connectWifi();
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
	setenv("TZ", Timezone, 1);  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
	tzset(); // Set the TZ environment variable
	btn.begin();
	btn.onPressed(smartConfig);

}

void loop()
{
	time_t t;
	static time_t last_t, last_weather;
	struct tm* tm;
	static const char* const wd[7] = { "Dim" ,"Lun","Mar","Mer","Jeu","Ven","Sam" };
	t = time(NULL);
	if (last_t == t) return; // draw each second
	last_t = t;
	tm = localtime(&t);
	if (t > (last_weather + 1500))
	{
		if (WiFi.status() != WL_CONNECTED)
			ESP.restart();
		obtain_wx_data();
		Serial.printf("\nt=%lu last_weather=%lu\n", (long)t, (long)last_weather);
		Serial.printf("maj:%02d:%02d", tm->tm_hour, tm->tm_min);
		last_weather = t;
	}
	matrix->fillScreen(0);
	//Heure:Min:sec
	matrix->setTextColor(matrix->color565(255, 255, 255));
	matrix->setFont(&FreeSansBold9pt7b);
	matrix->setCursor(0, 13);
	matrix->printf("%02d:%02d", tm->tm_hour, tm->tm_min);
	matrix->setFont();
	matrix->printf(":%02d", tm->tm_sec);
	// jour et date
	matrix->setCursor(2, 16);
	matrix->setTextColor(tm->tm_wday == 0 ? matrix->color565(0, 127, 255) :
		tm->tm_wday == 6 ? matrix->color565(127, 255, 127) :
		matrix->color565(127, 255, 127));
	matrix->printf("%s", wd[tm->tm_wday]);
	matrix->setCursor(2, 24);
	matrix->printf("%02d/%02d/%d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
	//Serial.printf("%02d/%02d/%04d\n", tm->tm_mday, tm->tm_mon + 1, tm->tm_year);
	//Temperature
	matrix->setTextColor(matrix->color565(255, 0, 0));
	if (WxConditions[0].Temperature < 10)
	{
		matrix->setCursor(38, 16);
	}
	else if (WxConditions[0].Temperature < 0) {
		matrix->setCursor(26, 16);
	}
	else matrix->setCursor(32, 16);
	//matrix->printf("%.1fC",-12.5);
	matrix->printf("%.1fC", WxConditions[0].Temperature);
	matrix->flipDMABuffer();
	btn.read();
}



