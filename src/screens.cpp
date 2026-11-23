#include "screens.h"
#include <WiFi.h>
#include "display.h"
#include "weather.h"
#include "weather_icons.h"
#include "ble_prov.h"
#include "config.h"
#include "settings.h"
#include "webui.h"
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/TomThumb.h>
#include <qrcode.h>
#include "Fonts/freesansbold14pt.h" // police de l'heure (include/Fonts)

#define CLOCK_FONT (&freesansbold14pt)

// ---------- Panneau du haut : HH:MM ----------
void drawClock(struct tm* tm, bool colonOn) {
	// Police à taille réelle. L'espacement d'origine de la 14pt donne 71 px pour "88:88" :
	// on place donc chaque caractère soi-même avec 2 px d'écart.
	// Chiffres : encre 13 px de large (xOffset 1) -> 13+2+13+2+4+2+13+2+13 = 64 px.
	const int digitX[4] = { 0, 15, 36, 51 };      // bord gauche de l'encre de chaque chiffre
	const int colonX = 30;                         // bord gauche de l'encre des ":" (4 px)
	const int baseline = (ZONE_H - 21) / 2 + 20;  // chiffres de 21 px de haut, centrés verticalement
	uint16_t c = col(C_HOUR);
	matrix->setFont(CLOCK_FONT);
	matrix->setTextSize(1);
	char buf[6];
	snprintf(buf, sizeof(buf), "%02d%02d", tm->tm_hour, tm->tm_min);
	GFXglyph* g8 = &CLOCK_FONT->glyph['8' - CLOCK_FONT->first];
	for (int i = 0; i < 4; i++)
		matrix->drawChar(digitX[i] - g8->xOffset, baseline, buf[i], c, c, 1);
	if (colonOn) {
		GFXglyph* gc = &CLOCK_FONT->glyph[':' - CLOCK_FONT->first];
		matrix->drawChar(colonX - gc->xOffset, baseline, ':', c, c, 1);
	}
	// Barres des secondes : 1 pas par seconde, pleine largeur à la 60e seconde, puis repart
	int bar = (tm->tm_sec + 1) * 64 / 60;
	uint16_t red = col(C_BARS);
	matrix->fillRect(0, 29, bar, 2, red);        // dessous : gauche -> droite
	matrix->fillRect(64 - bar, 1, bar, 2, red);  // dessus  : droite -> gauche
}

// ---------- Panneau du milieu : jour, date, température + humidité ----------
void drawDay(struct tm* tm, int oy) {
	static const char* const days[7] = { "Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi" };
	char buf[16];
	matrix->setFont();
	matrix->setTextColor(tm->tm_wday == 0 ? col(C_SUNDAY) : col(C_DAY));
	printCentered(days[tm->tm_wday], 0, 64, oy + 0);
	matrix->setTextColor(col(C_DATE));
	static const char* const months[12] = { "jan", "fev", "mars", "avr", "mai", "juin", "juil", "aout", "sept", "oct", "nov", "dec" };
	switch (settings.dateFmt) {
	case DATE_DMY_SHORT: snprintf(buf, sizeof(buf), "%02d/%02d/%02d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year % 100); break;
	case DATE_TEXT:      snprintf(buf, sizeof(buf), "%d %s", tm->tm_mday, months[tm->tm_mon]); break;
	case DATE_ISO:       snprintf(buf, sizeof(buf), "%d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday); break;
	default:             snprintf(buf, sizeof(buf), "%02d/%02d/%d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900); break;
	}
	printCentered(buf, 0, 64, oy + 9);

	// 3e ligne (oy+18..oy+30) : température à gauche, humidité à droite, en 9pt
	const int baseline = oy + 31; // chiffres 9pt : 13 px au-dessus de la ligne de base
	int16_t x1, y1; uint16_t w, h;
	matrix->setFont(&FreeSansBold9pt7b);
	if (!weatherOk) {
		matrix->setTextColor(matrix->color565(100, 100, 100));
		printCenteredGFX("--", 0, oy + 18, 64, 13);
		return;
	}
	// Température + "°" centrés dans [0, 32[
	snprintf(buf, sizeof(buf), "%d", (int)lroundf(weather.temperature));
	matrix->getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
	int x = (32 - (int)(w + 4)) / 2;
	uint16_t tc = weather.temperature < 0 ? col(C_TEMP_NEG) : col(C_TEMP);
	matrix->setTextColor(tc);
	matrix->setCursor(x - x1, baseline);
	matrix->print(buf);
	matrix->drawCircle(x + w + 2, oy + 19, 1, tc); // symbole degré
	// Humidité (chiffres 9pt + "%" petite police) centrés dans [32, 64[
	snprintf(buf, sizeof(buf), "%d", (int)lroundf(weather.humidity));
	matrix->getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
	x = 32 + (32 - (int)(w + 6)) / 2;
	matrix->setTextColor(col(C_HUM));
	matrix->setCursor(x - x1, baseline);
	matrix->print(buf);
	matrix->setFont();
	matrix->setCursor(x + w + 1, baseline - 8);
	matrix->print("%");
}

// Affiche une heure Unix en "HH:MM" (heure locale) avec une petite flèche haut/bas devant
static void drawSunTime(time_t ts, int y, bool up, uint16_t color) {
	struct tm st;
	localtime_r(&ts, &st);
	if (up) matrix->fillTriangle(26, y + 4, 30, y + 4, 28, y, color);
	else    matrix->fillTriangle(26, y, 30, y, 28, y + 4, color);
	matrix->drawFastHLine(26, y + 6, 5, color);
	char buf[6];
	snprintf(buf, sizeof(buf), "%02d:%02d", st.tm_hour, st.tm_min);
	matrix->setTextColor(color);
	matrix->setCursor(33, y);
	matrix->print(buf);
}

// ---------- Panneau du bas : icône, vent, lever/coucher du soleil ----------
// ---------- QR codes (réglage Bluetooth, Wi-Fi de réglage) ----------
// Le panneau ne fait que 32 px de haut : QR version 3 (29x29) max, donc 53 caractères max.
#define QR_MAX 29
static uint8_t qrSize = 0;                    // 0 = échec / pas de QR
static bool qrModules[QR_MAX][QR_MAX];
static String qrText;                         // texte du QR actuellement en mémoire

static void qrStore(esp_qrcode_handle_t q) {
	int n = esp_qrcode_get_size(q);
	if (n > QR_MAX) return;
	for (int y = 0; y < n; y++)
		for (int x = 0; x < n; x++) qrModules[y][x] = esp_qrcode_get_module(q, x, y);
	qrSize = n;
}

// Génère le QR de "text" (si ce n'est pas déjà celui en mémoire). Retourne false s'il est trop long.
static bool qrPrepare(const String& text) {
	if (text != qrText) {
		qrText = text;
		qrSize = 0;
		esp_qrcode_config_t cfg = { qrStore, 3, ESP_QRCODE_ECC_LOW };
		esp_qrcode_generate(&cfg, text.c_str());
		Serial.printf("QR (%d car.) -> %s\n", text.length(), qrSize ? "OK" : "trop long");
	}
	return qrSize != 0;
}

// Dessine le QR en mémoire à gauche du panneau (modules éteints sur fond blanc + 1 px de marge).
// Retourne la position x où commence la zone libre à droite.
static int drawQr(int oy) {
	const int x0 = 0, y0 = oy + (ZONE_H - (qrSize + 2)) / 2;
	matrix->fillRect(x0, y0, qrSize + 2, qrSize + 2, matrix->color565(255, 255, 255));
	for (int y = 0; y < qrSize; y++)
		for (int x = 0; x < qrSize; x++)
			if (qrModules[y][x]) matrix->drawPixel(x0 + 1 + x, y0 + 1 + y, 0);
	return qrSize + 3;
}

// Petit texte (police TomThumb 3x5) centré dans [x0, x0+w[, ligne de base y
static void printTiny(const char* txt, int x0, int w, int y) {
	matrix->setFont(&TomThumb);
	int16_t bx, by; uint16_t bw, bh;
	matrix->getTextBounds(txt, 0, 0, &bx, &by, &bw, &bh);
	matrix->setCursor(x0 + (w - (int)bw) / 2 - bx, y);
	matrix->print(txt);
	matrix->setFont();
}

// Réglage Bluetooth : QR de l'appli « ESP BLE Provisioning » + PIN.
// Le format Espressif complet ({"ver":"v1",...}) fait 64 car. : on retire "ver", facultatif.
static bool drawProvQr(int oy) {
	if (!qrPrepare(String("{\"name\":\"" PROV_NAME "\",\"pop\":\"" PROV_POP "\",\"transport\":\"ble\"}"))) return false;
	int xr = drawQr(oy), wr = 64 - xr;
	matrix->setFont();
	matrix->setTextColor(matrix->color565(0, 120, 255));
	printCentered("BLE", xr, wr, oy + 2);
	matrix->setTextColor(matrix->color565(200, 200, 200));
	printCentered("PIN", xr, wr, oy + 13);
	matrix->setTextColor(matrix->color565(255, 200, 0));
	printTiny(PROV_POP, xr, wr, oy + 29);
	return true;
}

// Wi-Fi de réglage : QR de connexion Wi-Fi (scan avec l'appareil photo) + mot de passe
static void drawApPanel(int oy) {
	String wifiQr = String("WIFI:T:WPA;S:") + webApSsid() + ";P:" + webApPassword() + ";;";
	if (qrPrepare(wifiQr)) {
		int xr = drawQr(oy), wr = 64 - xr;
		matrix->setFont();
		matrix->setTextColor(matrix->color565(0, 120, 255));
		printCentered("WiFi", xr, wr, oy + 2);
		matrix->setTextColor(matrix->color565(200, 200, 200));
		printTiny(webApSsid(), xr, wr, oy + 18);
		matrix->setTextColor(matrix->color565(255, 200, 0));
		printTiny(webApPassword(), xr, wr, oy + 27);
	} else { // secours si le QR ne tient pas : texte seul
		matrix->setFont();
		matrix->setTextColor(matrix->color565(200, 200, 200));
		printCentered(webApSsid(), 0, 64, oy + 4);
		matrix->setTextColor(matrix->color565(255, 200, 0));
		printCentered(webApPassword(), 0, 64, oy + 18);
	}
}

static void drawProvisioning(int oy) {
	if (provState == PROV_WAITING && drawProvQr(oy)) return;
	matrix->setFont();
	matrix->setTextColor(matrix->color565(0, 120, 255));
	printCentered("Bluetooth", 0, 64, oy + 2);
	switch (provState) {
	case PROV_WAITING:
		matrix->setTextColor(matrix->color565(200, 200, 200));
		printCentered(PROV_NAME, 0, 64, oy + 12);
		matrix->setTextColor(matrix->color565(255, 200, 0));
		printCentered("PIN " PROV_POP, 0, 64, oy + 22);
		break;
	case PROV_CONNECTING:
		matrix->setTextColor(matrix->color565(255, 200, 0));
		printCentered("Connexion", 0, 64, oy + 16);
		break;
	case PROV_SUCCESS:
		matrix->setTextColor(matrix->color565(0, 255, 0));
		printCentered("WiFi OK", 0, 64, oy + 16);
		break;
	case PROV_FAILED:
		matrix->setTextColor(matrix->color565(255, 0, 0));
		printCentered("Echec", 0, 64, oy + 12);
		printCentered("reessayer", 0, 64, oy + 22);
		break;
	default:
		break;
	}
}

// ---------- Message défilant ----------
// Les polices Adafruit n'ont pas d'accents : on les remplace par la lettre simple
static String toAscii(const String& s) {
	static const struct { const char* utf8; const char* ascii; } map[] = {
		{"à","a"},{"â","a"},{"ä","a"},{"é","e"},{"è","e"},{"ê","e"},{"ë","e"},{"î","i"},{"ï","i"},
		{"ô","o"},{"ö","o"},{"ù","u"},{"û","u"},{"ü","u"},{"ç","c"},{"œ","oe"},{"æ","ae"},
		{"À","A"},{"Â","A"},{"É","E"},{"È","E"},{"Ê","E"},{"Î","I"},{"Ô","O"},{"Ù","U"},{"Û","U"},{"Ç","C"},
		{"’","'"},{"‘","'"},{"«","\""},{"»","\""},{"€","EUR"},{"°"," deg"},
	};
	String out;
	for (unsigned i = 0; i < s.length();) {
		unsigned char c = s[i];
		if (c < 0x80) { out += (char)c; i++; continue; }
		bool found = false;
		for (auto& m : map) {
			size_t n = strlen(m.utf8);
			if (strncmp(s.c_str() + i, m.utf8, n) == 0) { out += m.ascii; i += n; found = true; break; }
		}
		if (!found) { // caractère inconnu : on saute toute la séquence UTF-8
			i++;
			while (i < s.length() && (s[i] & 0xC0) == 0x80) i++;
		}
	}
	return out;
}

static String  msgAscii;
static int     msgWidth = 0;
static uint32_t msgVersion = 0xFFFFFFFF;

static void msgPrepare() {
	if (msgVersion == settingsVersion) return;
	msgVersion = settingsVersion;
	msgAscii = toAscii(settings.message);
	matrix->setFont(&FreeSansBold9pt7b);
	int16_t bx, by; uint16_t bw, bh;
	matrix->getTextBounds(msgAscii.c_str(), 0, 0, &bx, &by, &bw, &bh);
	msgWidth = bw + bx;
}

// Position actuelle du défilement (en pixels) : sert aussi à savoir quand redessiner
static int msgOffset() {
	msgPrepare();
	uint32_t period = msgWidth + 64; // le texte entre par la droite et sort complètement à gauche
	return (uint64_t)millis() * settings.scrollSpeed / 1000 % period;
}

static void drawMessage(int oy) {
	msgPrepare();
	matrix->setFont(&FreeSansBold9pt7b);
	matrix->setTextWrap(false);
	matrix->setTextColor(col(C_MSG));
	matrix->setCursor(64 - msgOffset(), oy + 22); // majuscules sur les lignes 9..21, jambages jusqu'à 25
	matrix->print(msgAscii);
	matrix->setFont();
}

// Le panneau du bas affiche-t-il actuellement le message ?
// Le Wi-Fi de réglage est ouvert et personne n'y est encore connecté : on affiche le QR de connexion
static bool apWaitingForPhone() {
	return webApActive() && WiFi.softAPgetStationNum() == 0;
}

static bool bottomShowsMessage() {
	return provState == PROV_IDLE && !apWaitingForPhone() && settings.bottomMode == BOTTOM_MESSAGE;
}

int bottomAnimFrame() {
	return bottomShowsMessage() ? msgOffset() : -1;
}

// ---------- Panneau du bas ----------
void drawBottom(int oy) {
	if (provState != PROV_IDLE) {
		drawProvisioning(oy);
		return;
	}
	// Wi-Fi de réglage ouvert : QR de connexion jusqu'à ce qu'un téléphone se connecte
	// (ensuite le contenu normal revient, pour voir l'effet des réglages, et le mot de passe disparaît)
	if (apWaitingForPhone()) {
		drawApPanel(oy);
		return;
	}
	if (settings.bottomMode == BOTTOM_MESSAGE) {
		drawMessage(oy);
		return;
	}
	drawWeather(oy);
}

// ---------- Météo (panneau du bas) ----------
void drawWeather(int oy) {
	matrix->setFont();
	if (!weatherOk) {
		matrix->setTextColor(matrix->color565(255, 0, 0));
		printCentered(WiFi.status() == WL_CONNECTED ? "Meteo --" : "WiFi...", 0, 64, oy + 12);
		return;
	}
	drawWeatherIcon(0, oy + 4, weather.icon);

	// Zone droite (x = 25..63) : vent, lever, coucher du soleil
	char buf[12];
	matrix->setTextColor(col(C_WIND));
	snprintf(buf, sizeof(buf), "%dkm/h", (int)lroundf(weather.windspeed * 3.6f));
	printCentered(buf, 25, 39, oy + 2);
	drawSunTime(weather.sunrise, oy + 12, true,  col(C_SUNRISE));
	drawSunTime(weather.sunset,  oy + 22, false, col(C_SUNSET));
}
