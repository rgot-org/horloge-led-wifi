#include "settings.h"
#include "config.h"
#include "weather.h"
#include <Preferences.h>

const ColorDef colorDefs[C_COUNT] = {
	{ "hour",    "Heure" },
	{ "bars",    "Barres des secondes" },
	{ "day",     "Jour" },
	{ "sunday",  "Dimanche" },
	{ "date",    "Date" },
	{ "temp",    "Température" },
	{ "tempneg", "Température négative" },
	{ "hum",     "Humidité" },
	{ "wind",    "Vent" },
	{ "sunrise", "Lever du soleil" },
	{ "sunset",  "Coucher du soleil" },
	{ "msg",     "Message défilant" },
};

//                 heure     barres    jour      dimanche  date      temp      temp<0    humid.    vent      lever     coucher   message
const Theme themes[] = {
	{ "Classique", { 0xFFFFFF, 0xFF0000, 0x7FFF7F, 0x007FFF, 0xC8C8C8, 0xFF3C00, 0x00A0FF, 0x008CFF, 0x969696, 0xFFC800, 0xFF5A00, 0xFFFFFF } },
	{ "Nuit",      { 0xFF2000, 0x800000, 0xC01000, 0xFF4000, 0x900000, 0xFF3000, 0xFF6000, 0xC02000, 0x700000, 0xFF4000, 0xA01000, 0xFF2000 } },
	{ "Océan",     { 0x80E0FF, 0x0060FF, 0x40C0C0, 0x00FFA0, 0x6090B0, 0xFFB060, 0x80C0FF, 0x00A0FF, 0x5080A0, 0xFFE080, 0xFF8040, 0x80E0FF } },
	{ "Néon",      { 0xFF00FF, 0x00FFFF, 0x00FF60, 0xFFFF00, 0xB000FF, 0xFF0080, 0x00FFFF, 0x00FFC0, 0x8000FF, 0xFFFF00, 0xFF4000, 0x00FFFF } },
	{ "Ambre",     { 0xFFB000, 0xFF6000, 0xFFA000, 0xFFD060, 0xC08000, 0xFFC040, 0xFFE0A0, 0xE09000, 0xA06000, 0xFFD000, 0xFF8000, 0xFFB000 } },
	{ "Forêt",     { 0xC0FFC0, 0x40FF00, 0x80E060, 0xFFD000, 0x70A070, 0xFF9030, 0x60C0FF, 0x40C0A0, 0x608060, 0xFFE060, 0xFF7030, 0xA0FF80 } },
};
const int themeCount = sizeof(themes) / sizeof(themes[0]);

Settings settings;
uint32_t settingsVersion = 0;
bool     nightActive = false;

static Preferences prefs;
static unsigned long dirtyAt = 0;

void settingsApplyTheme(int i) {
	if (i < 0 || i >= themeCount) return;
	for (int c = 0; c < C_COUNT; c++) settings.color[c] = themes[i].color[c];
	settingsChanged();
}

// ---------- Thèmes personnels ----------
UserTheme userThemes[USER_THEME_MAX];

static void userThemeStore(int slot) { // écrit un emplacement en flash (opération rare : tout de suite)
	char key[8];
	snprintf(key, sizeof(key), "uth%d", slot);
	prefs.begin("horloge", false);
	if (userThemes[slot].name[0]) prefs.putBytes(key, &userThemes[slot], sizeof(UserTheme));
	else prefs.remove(key);
	prefs.end();
}

static void userThemesLoad() {
	prefs.begin("horloge", true);
	for (int s = 0; s < USER_THEME_MAX; s++) {
		char key[8];
		snprintf(key, sizeof(key), "uth%d", s);
		memset(&userThemes[s], 0, sizeof(UserTheme));
		// taille différente = format d'une ancienne version (nombre de couleurs changé) : ignoré
		if (prefs.isKey(key) && prefs.getBytesLength(key) == sizeof(UserTheme))
			prefs.getBytes(key, &userThemes[s], sizeof(UserTheme));
		userThemes[s].name[THEME_NAME_MAX] = 0;
	}
	prefs.end();
}

static int userThemeFind(const String& name) {
	for (int s = 0; s < USER_THEME_MAX; s++)
		if (userThemes[s].name[0] && name.equalsIgnoreCase(userThemes[s].name)) return s;
	return -1;
}

bool settingsApplyThemeByName(const String& name) {
	for (int i = 0; i < themeCount; i++)
		if (name.equalsIgnoreCase(themes[i].name)) { settingsApplyTheme(i); return true; }
	int s = userThemeFind(name);
	if (s < 0) return false;
	for (int c = 0; c < C_COUNT; c++) settings.color[c] = userThemes[s].color[c];
	settingsChanged();
	return true;
}

// Enregistre les couleurs actuelles sous "name" (remplace un thème personnel du même nom).
// Refusé si le nom est vide, trop long, celui d'un thème prédéfini, ou si les 4 emplacements sont pris.
bool userThemeSave(const String& rawName) {
	String name = rawName;
	name.trim();
	if (name.length() == 0 || name.length() > THEME_NAME_MAX) return false;
	for (int i = 0; i < themeCount; i++) if (name.equalsIgnoreCase(themes[i].name)) return false;
	int s = userThemeFind(name);
	if (s < 0) for (s = 0; s < USER_THEME_MAX && userThemes[s].name[0]; s++);
	if (s >= USER_THEME_MAX) return false;
	memset(&userThemes[s], 0, sizeof(UserTheme));
	strncpy(userThemes[s].name, name.c_str(), THEME_NAME_MAX);
	for (int c = 0; c < C_COUNT; c++) userThemes[s].color[c] = settings.color[c];
	userThemeStore(s);
	settingsVersion++; // la liste des thèmes a changé : page web et MQTT se mettent à jour
	return true;
}

bool userThemeDelete(const String& name) {
	int s = userThemeFind(name);
	if (s < 0) return false;
	userThemes[s].name[0] = 0;
	userThemeStore(s);
	settingsVersion++;
	return true;
}

void settingsReset() {
	settings.brightness  = DEFAULT_BRIGHTNESS;
	settings.nightOn     = false;
	settings.nightBright = 10;
	settings.bottomMode  = BOTTOM_WEATHER;
	settings.message     = "Bonjour !";
	settings.scrollSpeed = 25;
	settings.city        = OWM_CITY;
	settings.dateFmt     = DATE_DMY;
	settingsApplyTheme(0);
}

void settingsLoad() {
	settingsReset(); // valeurs par défaut pour tout ce qui n'a jamais été enregistré
	prefs.begin("horloge", true);
	settings.brightness  = max<uint8_t>(1, prefs.getUChar("bright", settings.brightness));
	settings.nightOn     = prefs.getBool("nightOn", settings.nightOn);
	settings.nightBright = max<uint8_t>(1, prefs.getUChar("nightBr", settings.nightBright));
	settings.bottomMode  = prefs.getUChar("bottom", settings.bottomMode);
	settings.message     = prefs.getString("msgText", settings.message);
	settings.scrollSpeed = prefs.getUChar("speed", settings.scrollSpeed);
	settings.city        = prefs.getString("city", settings.city);
	settings.dateFmt     = prefs.getUChar("dateFmt", settings.dateFmt) % DATE_FMT_COUNT;
	settings.encrypt     = prefs.getBool("encrypt", settings.encrypt);
	settings.uiPrefs     = prefs.getString("uiPrefs", "");
	for (int i = 0; i < C_COUNT; i++) settings.color[i] = prefs.getUInt(colorDefs[i].key, settings.color[i]);
	prefs.end();
	userThemesLoad();
	dirtyAt = 0; // rien à sauvegarder : on vient de lire
	settingsVersion++;
}

static void settingsSave() {
	prefs.begin("horloge", false);
	prefs.putUChar("bright", settings.brightness);
	prefs.putBool("nightOn", settings.nightOn);
	prefs.putUChar("nightBr", settings.nightBright);
	prefs.putUChar("bottom", settings.bottomMode);
	prefs.putString("msgText", settings.message);
	prefs.putUChar("speed", settings.scrollSpeed);
	prefs.putString("city", settings.city);
	prefs.putUChar("dateFmt", settings.dateFmt);
	prefs.putBool("encrypt", settings.encrypt);
	prefs.putString("uiPrefs", settings.uiPrefs);
	for (int i = 0; i < C_COUNT; i++) prefs.putUInt(colorDefs[i].key, settings.color[i]);
	prefs.end();
	Serial.println("Réglages enregistrés");
}

void settingsChanged() {
	settingsVersion++;
	dirtyAt = millis() | 1; // jamais 0
}

void settingsLoop() {
	if (dirtyAt && millis() - dirtyAt > 2000) {
		dirtyAt = 0;
		settingsSave();
	}
}

uint16_t col(ColorId id) {
	uint32_t c = settings.color[id];
	uint8_t r = c >> 16, g = c >> 8, b = c;
	return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Applique un réglage "clé = valeur" (page web et MQTT). Retourne false si la clé ou la valeur est invalide.
bool settingsApplyKey(const String& k, const String& v) {
	if (k == "bright")       settings.brightness  = constrain(v.toInt(), 1, 255);
	else if (k == "nightBr") settings.nightBright = constrain(v.toInt(), 1, 255);
	else if (k == "nightOn") settings.nightOn     = v.toInt() != 0 || v == "true";
	else if (k == "bottom")  settings.bottomMode  = (v.toInt() == 1 || v == "message") ? BOTTOM_MESSAGE : BOTTOM_WEATHER;
	else if (k == "speed")   settings.scrollSpeed = constrain(v.toInt(), 5, 80);
	else if (k == "dateFmt") settings.dateFmt     = constrain(v.toInt(), 0, DATE_FMT_COUNT - 1);
	else if (k == "msg")     settings.message     = v.substring(0, MESSAGE_MAX);
	else if (k == "encrypt") settings.encrypt     = v.toInt() != 0 || v == "true";
	else if (k == "uiPrefs") { // stocké tel quel pour la télécommande (taille limitée)
		if (v.length() > 300 || (v.length() && v[0] != '{')) return false;
		settings.uiPrefs = v;
	}
	else if (k == "city") {
		if (v.length() == 0 || v.length() >= 64) return false;
		settings.city = v;
		weatherRefresh = true; // la boucle principale relance la météo tout de suite
	}
	else if (k == "theme") { // nom (prédéfini ou personnel) ou numéro d'un thème prédéfini
		if (settingsApplyThemeByName(v)) return true;
		if (isDigit(v[0]) && v.toInt() < themeCount) { settingsApplyTheme(v.toInt()); return true; }
		return false;
	}
	else if (k == "saveTheme") return userThemeSave(v);   // enregistre les couleurs actuelles sous ce nom
	else if (k == "delTheme")  return userThemeDelete(v); // supprime un thème personnel
	else if (k.endsWith("_c")) { // couleur : "<clé>_c" = RRGGBB
		String hex = v.startsWith("#") ? v.substring(1) : v;
		if (hex.length() != 6) return false;
		String key = k.substring(0, k.length() - 2);
		int i = 0;
		while (i < C_COUNT && key != colorDefs[i].key) i++;
		if (i == C_COUNT) return false;
		settings.color[i] = strtoul(hex.c_str(), nullptr, 16);
	}
	else return false;
	settingsChanged();
	return true;
}
