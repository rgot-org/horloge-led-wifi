#pragma once
// Identifiants et clés secrètes : fichier séparé, exclu de Git (modèle : secrets.example.h)
#include "secrets.h"
// =====================================================================
//                     CONFIGURATION DE L'HORLOGE
// =====================================================================

// ---------- WiFi ----------

// Réseau par défaut, utilisé seulement si aucun réseau n'a été réglé par Bluetooth.

// ---------- Réglage du WiFi par Bluetooth (appui sur le bouton de la carte) ----------
// Appli « ESP BLE Provisioning » (Espressif, Android/iOS). Le nom doit commencer par "PROV_".
#define PROV_NAME       "PROV_HORL" // nom Bluetooth (9 car. max pour tenir sur l'écran)
#define PROV_POP        "123456"    // code PIN demandé par l'appli
#define PROV_TIMEOUT_MS (5UL * 60 * 1000) // abandon au bout de 5 min

// ---------- Wi-Fi privé de réglage (appui court sur le bouton) ----------
// La page de réglage n'est JAMAIS accessible depuis le réseau public : uniquement par ce Wi-Fi,
// protégé par un mot de passe aléatoire nouveau à chaque ouverture (affiché sur le panneau du bas).
#define AP_SSID             "HORLOGE"
#define AP_MAX_CLIENTS      1                    // un seul téléphone à la fois
#define AP_IDLE_TIMEOUT_MS  (10UL * 60 * 1000)   // fermeture après 10 min sans activité
#define AP_MAX_OPEN_MS      (30UL * 60 * 1000)   // et au plus tard après 30 min
#define AP_DISCONNECT_GRACE_MS 5000              // fermeture 5 s après la déconnexion du téléphone
#define BTN_LONG_PRESS_MS   5000                 // appui long : réglage du Wi-Fi par Bluetooth

// ---------- Mise à jour à distance par MQTT (messages signés) ----------
// Les commandes sont signées en HMAC-SHA256 avec MQTT_SECRET : sans la clé, impossible
// d'envoyer une commande acceptée. Horodatage + anti-rejeu : une commande interceptée ne peut
// pas être renvoyée. Générer une clé :  python -c "import secrets; print(secrets.token_hex(32))"
// et mettre LA MÊME dans l'outil d'envoi (tools/horloge_mqtt.py). Tant que la clé n'est pas
// changée, MQTT reste désactivé.
#define MQTT_BROKER      "broker.hivemq.com"
#define MQTT_PORT        1883               // 1883 = sans chiffrement, 8883 = TLS
// En clair : sur un broker public, tout abonné lit déjà les messages ; la sécurité vient de la
// signature HMAC. Et la RAM ne permet pas une connexion TLS MQTT permanente en plus de la météo.
#define MQTT_TLS         0
#define MQTT_DEVICE_ID   ""                 // vide = identifiant tiré de l'adresse MAC (affiché au démarrage)
#define MQTT_MAX_AGE_S   300                // une commande de plus de 5 min est refusée

// ---------- Météo (https://openweathermap.org/) ----------
#define OWM_CITY     "PARIS,FR"
#define OWM_LANG     "FR"
#define WEATHER_PERIOD_S 1500   // mise à jour météo toutes les 25 min

// ---------- Heure ----------
// Fuseau POSIX : https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIMEZONE     "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SERVER1  "0.europe.pool.ntp.org"
#define NTP_SERVER2  "time.nist.gov"

// ---------- Carte HD-WF2 : broches ----------
// Version de carte : 11 = v1.1, 15 = v1.5, 0 = HD-WF2 (V7.1.0)
#define WF2_VERSION 0

#if WF2_VERSION == 11
#define WF2_X1_R1_PIN 8
#define WF2_X1_R2_PIN 10
#define WF2_X1_G1_PIN 1
#define WF2_X1_G2_PIN 2
#define WF2_X1_B1_PIN 9
#define WF2_X1_B2_PIN 11
#define WF2_X1_E_PIN  3
#define WF2_A_PIN     12
#define WF2_B_PIN     4
#define WF2_C_PIN     14
#define WF2_D_PIN     5
#define WF2_OE_PIN    42
#define WF2_CLK_PIN   46
#define WF2_LAT_PIN   6
#elif WF2_VERSION == 15
#define WF2_X1_R1_PIN 12
#define WF2_X1_R2_PIN 46
#define WF2_X1_G1_PIN 11
#define WF2_X1_G2_PIN 10
#define WF2_X1_B1_PIN 14
#define WF2_X1_B2_PIN 9
#define WF2_X1_E_PIN  8
#define WF2_A_PIN     7
#define WF2_B_PIN     6
#define WF2_C_PIN     5
#define WF2_D_PIN     4
#define WF2_OE_PIN    1
#define WF2_CLK_PIN   3
#define WF2_LAT_PIN   2
#else // HD-WF2
#define WF2_X1_R1_PIN 2
#define WF2_X1_R2_PIN 3
#define WF2_X1_G1_PIN 6
#define WF2_X1_G2_PIN 7
#define WF2_X1_B1_PIN 10
#define WF2_X1_B2_PIN 11
#define WF2_X1_E_PIN  21
#define WF2_A_PIN     39
#define WF2_B_PIN     38
#define WF2_C_PIN     37
#define WF2_D_PIN     36
#define WF2_OE_PIN    35
#define WF2_CLK_PIN   34
#define WF2_LAT_PIN   33
#endif

#define WF2_BUTTON_TEST     17  // bouton sur le PCB, 1=normal, 0=appuyé
#define WF2_LED_RUN_PIN     40  // LED d'état
#define WF2_BM8563_I2C_SDA  41  // RTC BM8563
#define WF2_BM8563_I2C_SCL  42

// ---------- Panneaux ----------
#define PANEL_RES_X 64     // largeur d'un panneau
#define PANEL_RES_Y 32     // hauteur d'un panneau
#define PANEL_ROWS  3      // 3 panneaux empilés verticalement -> 64x96
#define PANEL_CHAIN PANEL_ROWS
// Câblage : 1 = ESP -> panneau du HAUT -> milieu -> bas ; 0 = ESP -> panneau du BAS -> milieu -> haut
#define PANEL_FIRST_AT_TOP 1
// Panneaux à retourner de 180° : bit0 = haut, bit1 = milieu, bit2 = bas (ex. 0b010 = milieu)
#define PANEL_FLIP_MASK    0b010

// Réglages anti-parasites
#define PANEL_CLKPHASE     false // front d'horloge
#define PANEL_LAT_BLANKING 2     // 1..4 : supprime les "fantômes" d'une ligne sur l'autre
#define PANEL_SOFT_EDGES   1     // 1 = sorties CLK/LAT/OE adoucies (équivalent d'une résistance série)
