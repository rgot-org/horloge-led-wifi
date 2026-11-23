#pragma once
#include <Arduino.h>

// Réglages modifiables depuis l'interface web, mémorisés dans la flash (NVS)

enum ColorId : uint8_t {
	C_HOUR, C_BARS, C_DAY, C_SUNDAY, C_DATE, C_TEMP, C_TEMP_NEG, C_HUM, C_WIND, C_SUNRISE, C_SUNSET, C_MSG,
	C_COUNT
};

struct ColorDef {
	const char* key;   // identifiant (web + NVS, 13 car. max)
	const char* label; // libellé affiché dans l'interface
};
extern const ColorDef colorDefs[C_COUNT];

// Thèmes : un jeu complet de couleurs (dans l'ordre de ColorId)
struct Theme {
	const char* name;
	uint32_t    color[C_COUNT];
};
extern const Theme themes[];
extern const int   themeCount;

enum BottomMode : uint8_t { BOTTOM_WEATHER = 0, BOTTOM_MESSAGE = 1 };
enum DateFormat : uint8_t { DATE_DMY = 0, DATE_DMY_SHORT, DATE_TEXT, DATE_ISO, DATE_FMT_COUNT };

struct Settings {
	uint8_t  brightness;     // luminosité de jour 1..255
	bool     nightOn;        // luminosité réduite entre le coucher et le lever du soleil
	uint8_t  nightBright;    // luminosité de nuit 1..255
	uint8_t  bottomMode;     // BottomMode : contenu du panneau du bas
	String   message;        // message défilant
	uint8_t  scrollSpeed;    // vitesse de défilement en pixels/s
	String   city;           // ville pour la météo, ex. "PARIS,FR"
	uint8_t  dateFmt;        // DateFormat
	bool     encrypt;        // chiffrer les échanges MQTT (commandes, réponses, état)
	String   uiPrefs;        // préférences de la télécommande (JSON opaque : taille du texte, cartes repliées)
	uint32_t color[C_COUNT]; // 0xRRGGBB
};
extern Settings settings;
extern uint32_t settingsVersion; // incrémenté à chaque changement (pour redessiner)
extern bool     nightActive;     // la luminosité de nuit est actuellement appliquée

#define DEFAULT_BRIGHTNESS 63
#define MESSAGE_MAX        150

void     settingsLoad();
void     settingsChanged();          // après une modification : redessin + sauvegarde différée
void     settingsReset();            // valeurs par défaut
void     settingsApplyTheme(int i);  // applique les couleurs d'un thème prédéfini

// Thèmes personnels : couleurs actuelles enregistrées sous un nom (en flash)
#define USER_THEME_MAX 4
#define THEME_NAME_MAX 16
struct UserTheme {
	char     name[THEME_NAME_MAX + 1]; // vide = emplacement libre
	uint32_t color[C_COUNT];
};
extern UserTheme userThemes[USER_THEME_MAX];
bool settingsApplyThemeByName(const String& name); // thème prédéfini ou personnel
bool userThemeSave(const String& name);   // enregistre les couleurs actuelles sous ce nom
bool userThemeDelete(const String& name); // supprime un thème personnel
void     settingsLoop();             // sauvegarde 2 s après la dernière modification (ménage la flash)
bool     settingsApplyKey(const String& k, const String& v); // "clé = valeur" (web, MQTT)
uint16_t col(ColorId id);            // couleur au format des panneaux (RGB565)
