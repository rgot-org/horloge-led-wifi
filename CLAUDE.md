# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Horloge Wi-Fi : firmware Arduino/PlatformIO pour carte **HD-WF2 (ESP32-S3)** pilotant **3 panneaux HUB75 64×32 chaînés et empilés** (écran virtuel 64×96), plus des outils PC (télécommande MQTT, installateur Windows). Le code, les commentaires et les échanges avec l'utilisateur sont en français.

## Commandes

```bash
pio run                      # compiler (seule vérification possible : pas de tests, pas de lint)
pio device monitor -b 115200 # moniteur série (identifiant MQTT, IP, mémoire libre, erreurs)
```

- **Ne jamais lancer `pio run -t upload`** : l'utilisateur téléverse lui-même (le téléversement lancé par Claude plante la carte). Compiler, puis dire que c'est prêt à téléverser.
- Rien ne peut être testé sur le matériel depuis ici : le signaler honnêtement.
- `test/` est le dossier vide par défaut de PlatformIO.
- `original/main.cpp` est la version d'avant toutes les modifications (hors `src/`, donc non compilée).

Outils PC :

```powershell
powershell -ExecutionPolicy Bypass -File tools\make_setup.ps1 -DeviceId <id> -Out dist\setup.exe  # installateur Windows
python tools/horloge_mqtt.py --id <id> [--encrypt] cle=valeur ...                                   # commande MQTT en ligne de commande
```

`make_setup.ps1` lit `MQTT_SECRET` dans `src/secrets.h` et l'intègre au `setup.exe` : **`dist/` contient la clé**. Après toute modification de `tools/horloge_ihm.html`, il faut refaire le setup pour que l'application installée en profite.

## Architecture du firmware (`src/`)

- **Secrets** (Wi-Fi par défaut, clé OpenWeatherMap, `MQTT_SECRET`) : `src/secrets.h`, inclus par `config.h` et **exclu de Git** (modèle versionné : `src/secrets.example.h`). Ne jamais remettre de secret dans un fichier suivi.
- **`config.h`** regroupe les autres réglages de compilation : broker MQTT, broches HD-WF2, câblage des panneaux (`PANEL_FIRST_AT_TOP`, `PANEL_FLIP_MASK`), réglages anti-parasites (`PANEL_CLKPHASE false` + `PANEL_SOFT_EDGES` sont les valeurs validées sur le matériel).
- **Boucle d'affichage** (`main.cpp` → `render()`) : on ne redessine que si quelque chose change (seconde, « : » clignotant à 1 Hz, `weatherVersion`, `settingsVersion`, état Wi-Fi/Bluetooth/AP, image du défilement via `bottomAnimFrame()`). Chaque image = `fillScreen` + `drawClock` (panneau du haut, y 0–31) + `drawDay` (y 32–63) + `drawBottom` (y 64–95) + `flipDMABuffer`. `render()` est aussi appelée pendant l'attente du Wi-Fi au démarrage.
- **`display.cpp` / `PanelStack`** : remplace `VirtualMatrixPanel`. Convertit (x,y) 64×96 en position dans la chaîne DMA 192×32 et retourne les panneaux de `PANEL_FLIP_MASK`. `fillScreen()` attend la fin d'un rafraîchissement après chaque `flipDMABuffer()` : sur ESP32-S3 la bascule de tampon n'a lieu qu'en fin d'image, dessiner trop tôt provoque un clignotement.
- **`settings.cpp`** est le point central des réglages modifiables : `settingsApplyKey(clé, valeur)` est la **seule** porte d'entrée, partagée par la page web et MQTT. Toute modification incrémente `settingsVersion` (→ redessin + republication MQTT) et déclenche une sauvegarde NVS différée de 2 s. Thèmes prédéfinis (`themes[]`) + 4 thèmes personnels (blobs NVS `uth0..3`, taille liée à `C_COUNT` : ajouter une couleur invalide les thèmes enregistrés). Pour ajouter un réglage : champ dans `Settings`, load/save, branche dans `settingsApplyKey`, champ dans `buildSettingsJson()` (`webui.cpp`), puis l'interface dans les **deux** pages (locale et `horloge_ihm.html`).
- **`uiPrefs`** : texte JSON opaque (≤ 300 car.) stocké par l'horloge pour la télécommande (taille du texte, cartes repliées). Le firmware ne l'interprète pas ; la page le valide (`parsePrefs`).
- **`buildSettingsJson()`** (`webui.cpp`) produit l'état complet servi à la page locale ET publié sur MQTT (`horloge/<id>/state`, retenu).
- **Heure** (`timekeeping.cpp`) : RTC BM8563 (I2C 0x51, SDA 41 / SCL 42) lue au démarrage avant le Wi-Fi, réécrite après chaque synchro NTP. La RTC stocke l'**UTC** ; le fuseau POSIX `TIMEZONE` gère l'heure d'été (`configTime` écrase TZ, d'où le `setenv` après).
- **Météo** (`weather.cpp`) : OpenWeatherMap en HTTPS, ville dans `settings.city`, toutes les 25 min ou immédiatement via `weatherRefresh`.

## Modèle de sécurité (l'horloge est sur un réseau Wi-Fi public)

- **Page de réglage locale** (`webui.cpp`) : jamais exposée sur le réseau public. Appui court sur le bouton (GPIO 17) → point d'accès privé `HORLOGE`, mot de passe WPA2 aléatoire à chaque ouverture (QR code de connexion sur le panneau du bas), 1 client max, portail captif, fermeture 5 s après déconnexion du téléphone / 10 min d'inactivité / 30 min max. `allowed()` refuse toute requête qui n'arrive pas par l'AP.
- **Appui long (5 s)** : réglage du Wi-Fi par Bluetooth (`ble_prov.cpp`, WiFiProv + appli « ESP BLE Provisioning ». BluFi n'est pas compilé dans le framework Arduino).
- **MQTT** (`mqtt_remote.cpp`, broker public, en clair sur 1883) : toute commande est signée HMAC-SHA256 avec `MQTT_SECRET` (≥ 16 caractères, sinon MQTT désactivé). Anti-rejeu par horodatage **en millisecondes**, strictement croissant, mémorisé en NVS, fenêtre de 5 min.
- **Formats de message**, à garder identiques dans **trois implémentations** (firmware, JS de `tools/horloge_ihm.html`, `tools/horloge_mqtt.py`) :
  - en clair : `<json>.<hmac hex>` ;
  - chiffré (réglage `encrypt`) : `E1.<IV b64>.<données b64>.<hmac hex>`, AES-256-CTR (compteur 128 bits), clé AES = `HMAC-SHA256(MQTT_SECRET, "horloge-enc")`, HMAC calculé sur tout ce qui précède le dernier point (chiffrer puis signer).
- La signature HMAC est en SHA-256 JavaScript pur dans la page (pour fonctionner en `file://`) ; le chiffrement utilise WebCrypto.

## Contraintes à connaître

- **RAM** : pas de PSRAM et les tampons DMA des 3 panneaux sont gros. Deux connexions TLS simultanées échouent (`SSL - Memory allocation failed`). C'est pourquoi MQTT est en clair (`MQTT_TLS 0`) ; si on le remet en TLS, `mqttSuspend()` le coupe pendant la requête météo.
- **Polices** : l'heure utilise `include/Fonts/freesansbold14pt.h` (variable `freesansbold14pt`) avec placement manuel des chiffres (l'espacement natif fait 71 px > 64). Le reste utilise FreeSansBold9pt7b, la police par défaut 6×8 et TomThumb. Les polices Adafruit n'ont pas d'accents : le message défilant passe par `toAscii()`.
- **QR codes sur 32 px de haut** : version 3 max (29×29, 53 caractères). Le QR « ESP BLE Provisioning » omet donc le champ `"ver"`.
- **Windows insensible à la casse** : un fichier `src/network.h` masquerait le `Network.h` du framework (d'où `wifi_conn.*`). Vérifier les noms de nouveaux fichiers.
- **Installateur** : IExpress (fourni avec Windows) échoue sans message si le chemin du `.sed` contient des `\` ou si `InstallPrompt` contient du texte. Les `.ps1` doivent être en UTF-8 **avec BOM** pour PowerShell 5 (accents).
- **`tools/horloge_ihm.html`** est une page autonome (MQTT par WebSocket `wss://broker.hivemq.com:8884/mqtt`). Elle ne peut pas être publiée comme Artifact claude.ai (la CSP bloque le broker). `const PRESET=null;` est le marqueur remplacé par `make_setup.ps1`.
