# Horloge Wi-Fi

Horloge connectée à LED : trois panneaux 64×32 empilés (écran de 64×96 pixels) pilotés par une carte **HD-WF2 (ESP32-S3)**. Elle affiche l'heure, la date, la météo ou un message défilant, et se règle sans fil depuis un téléphone ou un ordinateur.

```text
┌──────────────────┐
│     10:42        │  Panneau du haut   : heure, « : » qui clignotent, barres des secondes
├──────────────────┤
│     Lundi        │  Panneau du milieu : jour, date, température, humidité
│   23/11/2026     │
│   8°      87%    │
├──────────────────┤
│ ☁  14km/h        │  Panneau du bas    : météo (icône, vent, lever/coucher du soleil)
│    ▲ 08:12       │                      ou message défilant
│    ▼ 16:58       │
└──────────────────┘
```

---

## Sommaire

1. [Mode d'emploi](#1-mode-demploi)
2. [Régler l'horloge sur place](#2-régler-lhorloge-sur-place-wi-fi-privé)
3. [Régler l'horloge à distance](#3-régler-lhorloge-à-distance-télécommande)
4. [Changer le réseau Wi-Fi](#4-changer-le-réseau-wi-fi-bluetooth)
5. [Sécurité](#5-sécurité)
6. [Dépannage](#6-dépannage)
7. [Installation et compilation (pour le responsable)](#7-installation-et-compilation-pour-le-responsable)
8. [Organisation du projet](#8-organisation-du-projet)

---

## 1. Mode d'emploi

### Ce qui s'affiche

| Panneau | Contenu |
| --- | --- |
| Haut | L'heure en grands chiffres. Les « : » clignotent chaque seconde. Deux barres rouges avancent d'un pas par seconde et repartent à zéro chaque minute. |
| Milieu | Le jour (en bleu le dimanche), la date, la température et l'humidité. |
| Bas | Au choix : la météo (icône du temps, vent, heures de lever et de coucher du soleil) **ou** un message défilant. |

### À l'allumage

1. L'heure s'affiche tout de suite grâce à l'horloge interne de la carte (RTC), même sans Internet.
2. Pendant la connexion au Wi-Fi, le panneau du bas indique « WiFi… ».
3. Une fois connectée, l'horloge remet son heure à jour sur Internet et télécharge la météo (toutes les 25 minutes ensuite).

Sans Wi-Fi, l'horloge continue de fonctionner. Seule la météo affiche « -- ». Elle retente la connexion toutes les minutes.

### Le bouton de la carte

| Appui | Action |
| --- | --- |
| **Court** | Ouvre (ou ferme) le Wi-Fi privé de réglage. Voir [section 2](#2-régler-lhorloge-sur-place-wi-fi-privé). |
| **Long (5 secondes)** | Réglage du réseau Wi-Fi par Bluetooth. Voir [section 4](#4-changer-le-réseau-wi-fi-bluetooth). Un nouvel appui long annule. |

### Ce que l'on peut régler

Tous ces réglages sont disponibles sur place comme à distance, et sont conservés après une coupure de courant :

- **Thème** : 6 jeux de couleurs prêts à l'emploi (Classique, Nuit, Océan, Néon, Ambre, Forêt), plus 4 thèmes personnels à enregistrer soi-même.
- **Couleurs** : une couleur par élément (heure, barres, jour, dimanche, date, température, température négative, humidité, vent, lever, coucher, message).
- **Luminosité** de jour, et **luminosité de nuit automatique** entre le coucher et le lever du soleil.
- **Panneau du bas** : météo ou message défilant (texte jusqu'à 150 caractères, couleur, vitesse).
- **Ville** de la météo, au format `Ville,Pays` (ex. `LYON,FR`).
- **Format de la date** : `22/11/2026`, `22/11/26`, `22 nov` ou `2026-11-22`.

> Les panneaux n'ont pas d'accents : « Fête à 18h » s'affiche « Fete a 18h ».

---

## 2. Régler l'horloge sur place (Wi-Fi privé)

Pour quelqu'un qui est devant l'horloge, avec un téléphone.

1. **Appui court** sur le bouton de la carte.
2. Le panneau du bas affiche un **QR code** et, à côté, le nom du réseau (`HORLOGE`) et un mot de passe.
3. **Scannez le QR code** avec l'appareil photo du téléphone et acceptez de rejoindre le réseau.
4. La page de réglage s'ouvre toute seule. Sinon, ouvrez `http://192.168.4.1` dans le navigateur.
5. Faites vos réglages : ils s'appliquent tout de suite.
6. Quittez le réseau `HORLOGE` sur le téléphone : l'horloge coupe son Wi-Fi privé 5 secondes plus tard.

Bon à savoir :

- Le mot de passe change **à chaque ouverture** et disparaît de l'écran dès que le téléphone est connecté.
- Un seul téléphone peut se connecter à la fois.
- Le Wi-Fi privé se ferme aussi tout seul après 10 minutes sans activité (30 minutes au maximum), ou par un nouvel appui court.
- La page locale affiche aussi l'**identifiant MQTT** de l'horloge (utile pour la télécommande).

---

## 3. Régler l'horloge à distance (télécommande)

La télécommande permet de tout régler depuis n'importe où, par Internet. Elle passe par un service de messagerie public (broker MQTT) ; chaque commande est **signée** avec une clé secrète, donc personne d'autre ne peut modifier l'horloge.

### Installer la télécommande (Windows)

Le responsable fournit un fichier `setup.exe` déjà configuré.

1. Double-cliquez sur `setup.exe`.
2. Windows affiche probablement « **Windows a protégé votre ordinateur** » : cliquez sur **Informations complémentaires**, puis **Exécuter quand même** (le fichier n'est pas signé numériquement, c'est normal).
3. Répondez **Oui** à « Installer la télécommande de l'horloge ? ».
4. Une icône **Horloge** apparaît sur le Bureau et dans le menu Démarrer. La télécommande s'ouvre et se connecte toute seule.

Pour la désinstaller : **Paramètres → Applications → Applications installées → Horloge (télécommande)**.

Pour la mettre à jour : relancez simplement le nouveau `setup.exe` (fermez la télécommande avant).

### Utiliser la télécommande

- En haut à droite, deux pastilles indiquent si la télécommande est connectée au **broker** et si l'**horloge** est en ligne.
- L'**aperçu** montre l'écran de l'horloge tel qu'il sera, en direct.
- Chaque changement est **envoyé automatiquement**. L'indicateur en haut passe de « Envoi… » à « Enregistré ». En cas de problème, le **journal** en bas indique la raison.
- **Enregistrer un thème** : réglez les couleurs, tapez un nom dans la carte « Thème » et cliquez sur **Enregistrer**. La petite **×** supprime un thème personnel.
- **Chiffrer les échanges** (carte Connexion) : rend le contenu des messages illisible pour les autres utilisateurs du broker public. Recommandé.
- **Taille du texte** : boutons **A− / A+** en haut à droite.
- **Replier une carte** : cliquer sur son titre (▾ / ▸), par exemple pour masquer « Couleurs » ou « Journal ».
- La taille du texte et les cartes repliées sont **enregistrées dans l'horloge** : on retrouve la même présentation sur n'importe quel poste (elle est commune à toutes les télécommandes de cette horloge).

### Sans installation

La page `tools/horloge_ihm.html` peut aussi être ouverte directement dans un navigateur (Edge, Chrome, Firefox). Il faut alors saisir l'identifiant de l'horloge et la clé secrète dans la carte Connexion.

### En ligne de commande (script Python)

```bash
pip install paho-mqtt cryptography
set HORLOGE_SECRET=<la clé secrète>
python tools/horloge_mqtt.py --id <identifiant> msg="Fermeture à 18h" bottom=message theme=Nuit
python tools/horloge_mqtt.py --id <identifiant> --encrypt bright=40
```

Réglages possibles : `bright`, `nightOn`, `nightBr`, `bottom` (`weather`/`message`), `msg`, `speed`, `city`, `dateFmt`, `theme`, `saveTheme`, `delTheme`, `encrypt`, et `<couleur>_c=RRGGBB` (couleurs : `hour bars day sunday date temp tempneg hum wind sunrise sunset msg`). La liste complète est en tête du script.

---

## 4. Changer le réseau Wi-Fi (Bluetooth)

À faire si l'horloge change de lieu ou si le mot de passe du Wi-Fi change.

1. Installez sur le téléphone l'appli gratuite **« ESP BLE Provisioning »** (Espressif, Android et iOS).
2. **Appui long (5 s)** sur le bouton de la carte. Le panneau du bas affiche un QR code, « BLE » et un code PIN.
3. Dans l'appli : **Provision Device**, puis scannez le QR code (ou « I don't have a QR code », appareil `PROV_HORL`, puis saisissez le PIN).
4. Choisissez le réseau Wi-Fi et tapez son mot de passe.
5. Le panneau affiche « Connexion », puis « WiFi OK », et la carte redémarre sur le nouveau réseau.

En cas d'erreur de mot de passe, le panneau affiche « Echec » : recommencez depuis l'appli. Un nouvel appui long, ou 5 minutes sans rien faire, annule et revient à l'ancien réseau.

---

## 5. Sécurité

L'horloge est prévue pour un lieu public, sur un réseau Wi-Fi public.

- **Rien n'est accessible depuis le réseau public.** La page de réglage n'existe que sur le Wi-Fi privé de l'horloge, ouvert par un appui sur le bouton : il faut un accès physique à la carte. **Protégez le bouton** (boîtier fermé).
- **Commandes à distance signées** : sans la clé secrète, impossible de modifier l'horloge. Une commande interceptée ne peut pas être renvoyée (horodatage, validité 5 minutes).
- **Chiffrement** (option) : rend le contenu des messages illisible sur le broker public.
- **Le `setup.exe` contient la clé secrète.** Toute personne qui le possède peut régler l'horloge. Ne le transmettez qu'aux personnes autorisées. Pour retirer l'accès à quelqu'un : changer la clé, reflasher l'horloge, refaire et redistribuer le setup.
- **Code PIN Bluetooth** : changez la valeur par défaut (`PROV_POP` dans `config.h`).

---

## 6. Dépannage

| Symptôme | Cause probable / solution |
| --- | --- |
| La télécommande indique « Horloge : hors ligne » mais fonctionne | Message périmé d'une ancienne connexion (après un redémarrage). Se corrige seul en moins d'une minute ou au premier réglage. |
| « Refusé : horodatage trop ancien ou dans le futur » | L'heure du PC ou du téléphone est fausse. La remettre à l'heure automatique. |
| « Refusé : signature invalide » | La clé saisie dans la télécommande ne correspond pas à celle de l'horloge. |
| « Pas de réponse de l'horloge » | Horloge éteinte, sans Wi-Fi, ou mauvais identifiant. |
| « État rejeté » dans le journal | L'horloge chiffre ses messages mais la télécommande est trop ancienne : installer le dernier `setup.exe`. |
| Météo « -- » | Pas de Wi-Fi, ou ville introuvable (la télécommande l'indique dans la carte Météo). |
| « Meteo -- » longtemps alors que le Wi-Fi marche | Voir le moniteur série : `SSL - Memory allocation failed` signale un manque de mémoire (voir section 7). |
| Parasites sur les panneaux | Vérifier l'alimentation (une par panneau, masse commune) et les nappes. Réglages logiciels dans `config.h` : `PANEL_CLKPHASE`, `PANEL_LAT_BLANKING`, `PANEL_SOFT_EDGES`. |
| L'heure est fausse après une coupure de courant longue | La carte n'a pas de pile de sauvegarde pour la RTC : l'heure revient dès la connexion Wi-Fi. |
| La page `http://192.168.4.1` ne s'ouvre pas | Vérifier que le téléphone est bien sur le réseau `HORLOGE` (désactiver les données mobiles si besoin). |

---

## 7. Installation et compilation (pour le responsable)

### Matériel

- Carte **HD-WF2** (ESP32-S3, RTC BM8563, bouton sur GPIO 17).
- **3 panneaux HUB75 64×32** chaînés (sortie du premier vers l'entrée du deuxième, etc.) et empilés verticalement. Prévoir une alimentation 5 V suffisante (environ 4 A par panneau) et une masse commune.

### Logiciels

- [VS Code](https://code.visualstudio.com/) + extension **PlatformIO**.
- Python 3 (pour le script et la génération de clé), facultatif : `pip install paho-mqtt cryptography`.

### Secrets : `src/secrets.h` (à créer)

Ce fichier n'est **pas** dans le dépôt Git (il est exclu par `.gitignore`). Après avoir récupéré le projet :

1. Copier `src/secrets.example.h` en `src/secrets.h`.
2. Y renseigner :

| Réglage | Rôle |
| --- | --- |
| `DEFAULT_SSID`, `PASS` | Wi-Fi utilisé tant qu'aucun réseau n'a été réglé par Bluetooth. |
| `OWM_API_KEY` | Clé [OpenWeatherMap](https://openweathermap.org/) (gratuite). |
| `MQTT_SECRET` | **Clé secrète** de la télécommande (au moins 16 caractères, sinon la commande à distance est désactivée). |

### Configuration : `src/config.h`

| Réglage | Rôle |
| --- | --- |
| `OWM_CITY` | Ville par défaut de la météo. |
| `TIMEZONE` | Fuseau horaire au format POSIX (heure d'été comprise). |
| `MQTT_DEVICE_ID` | Identifiant de l'horloge ; vide = tiré de l'adresse MAC (affiché au démarrage). |
| `PROV_NAME`, `PROV_POP` | Nom et code PIN du réglage Bluetooth. |
| `PANEL_FIRST_AT_TOP`, `PANEL_FLIP_MASK` | Câblage des panneaux (sens de la chaîne, panneaux retournés). |

Générer une clé secrète :

```bash
python -c "import secrets; print(secrets.token_hex(32))"
```

### Compiler et téléverser

```bash
pio run                       # compiler
pio run -t upload             # téléverser (carte branchée en USB)
pio device monitor -b 115200  # moniteur série
```

Au démarrage, le moniteur série affiche notamment : l'heure lue dans la RTC, l'adresse IP, `MQTT : identifiant de l'horloge = …` et la mémoire libre.

### Fabriquer le `setup.exe` de la télécommande

```powershell
powershell -ExecutionPolicy Bypass -File tools\make_setup.ps1 -DeviceId <identifiant> -Out dist\setup.exe
```

La clé est lue directement dans `src/secrets.h`. Le fichier produit (`dist\setup.exe`, environ 300 Ko) contient la page pré-configurée, la bibliothèque MQTT et l'icône. **Refaire le setup** après chaque modification de `tools/horloge_ihm.html` ou changement de clé. Le dossier `dist/` contient la clé : ne pas le partager.

### Limites connues

- **Mémoire** : la carte n'a pas de PSRAM. Deux connexions chiffrées (TLS) simultanées sont impossibles : MQTT est donc en clair vers le broker (la sécurité repose sur la signature et le chiffrement optionnel des messages).
- **Port 1883 bloqué** par le réseau : repasser `MQTT_PORT 8883` / `MQTT_TLS 1` (MQTT est alors coupé pendant chaque requête météo).
- La télécommande a besoin d'Internet et d'un navigateur récent (WebCrypto pour le chiffrement).

---

## 8. Organisation du projet

```text
src/                    Firmware
  main.cpp              Démarrage et boucle principale (affichage, météo, MQTT)
  config.h              Tous les réglages de compilation
  secrets.example.h     Modèle des secrets (à copier en secrets.h, exclu de Git)
  display.*             Écran virtuel 64×96 sur les 3 panneaux
  screens.*             Dessin des 3 panneaux, QR codes, message défilant
  weather_icons.*       Icônes météo
  settings.*            Réglages modifiables, thèmes, sauvegarde en flash
  webui.*               Wi-Fi privé et page de réglage locale
  mqtt_remote.*         Commande à distance (signature, chiffrement, anti-rejeu)
  ble_prov.*            Réglage du Wi-Fi par Bluetooth
  wifi_conn.*           Connexion Wi-Fi
  timekeeping.*         RTC et NTP
  weather.*             Météo OpenWeatherMap
include/Fonts/          Police de l'heure (14 pt)
tools/
  horloge_ihm.html      Télécommande (page autonome)
  horloge_mqtt.py       Commande en ligne de commande
  make_setup.ps1        Fabrication du setup.exe
  installer/            Scripts d'installation / désinstallation
original/main.cpp       Version d'origine du programme (référence, non compilée)
CLAUDE.md               Notes techniques pour l'assistant de développement
```
