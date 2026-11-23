#pragma once
// MODÈLE des secrets de l'horloge.
// Copier ce fichier en "secrets.h" (même dossier) et remplacer les valeurs.
// secrets.h est exclu de Git : vos identifiants ne seront jamais publiés.

// Wi-Fi utilisé tant qu'aucun réseau n'a été réglé par Bluetooth
#define DEFAULT_SSID "MonReseauWiFi"
#define PASS         "MonMotDePasseWiFi"

// Clé OpenWeatherMap (gratuite) : https://openweathermap.org/api
#define OWM_API_KEY  "VOTRE_CLE_OPENWEATHERMAP"

// Clé secrète de la télécommande MQTT (au moins 16 caractères, sinon MQTT est désactivé).
// Générer :  python -c "import secrets; print(secrets.token_hex(32))"
#define MQTT_SECRET  "CHANGEZ_MOI"
