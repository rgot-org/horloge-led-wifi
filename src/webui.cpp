#include "webui.h"
#include "config.h"
#include "settings.h"
#include "weather.h"
#include "mqtt_remote.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <esp_random.h>

static WebServer server(80);
static DNSServer dns;            // portail captif : tous les noms -> l'horloge
static bool apActive = false;
static unsigned long apLastActivity = 0, apOpenedAt = 0, apEmptySince = 0;
static bool apHadClient = false; // un téléphone s'est connecté pendant cette session
static char apPass[9];           // mot de passe WPA2 aléatoire (8 caractères)

// ---------- Page web (stockée en flash) ----------
static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Horloge</title>
<style>
:root{--bg:#111418;--card:#1b2027;--line:#2a313b;--txt:#e8ebef;--mut:#8b95a3;--acc:#ff4d3d;--ok:#4cd07d;--bad:#ff6b5e}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--txt);font:16px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
main{max-width:520px;margin:0 auto;padding:20px 16px 40px}
h1{font-size:1.4rem;margin:0 0 4px}
p.sub{margin:0 0 20px;color:var(--mut);font-size:.9rem}
section{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:6px 16px 10px;margin-bottom:16px}
h2{font-size:.8rem;text-transform:uppercase;letter-spacing:.08em;color:var(--mut);margin:12px 0 4px}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:10px 0;border-top:1px solid var(--line)}
h2+.row,h2+.themes{border-top:0}
label{flex:1}
.hint{font-size:.8rem;color:var(--mut);margin:-4px 0 6px}
input[type=color]{width:56px;height:36px;border:1px solid var(--line);border-radius:8px;background:none;padding:2px;cursor:pointer}
input[type=range]{flex:1;accent-color:var(--acc)}
input[type=checkbox]{width:22px;height:22px;accent-color:var(--acc)}
input[type=text],select{flex:1;min-width:0;padding:9px 10px;border-radius:8px;border:1px solid var(--line);background:var(--bg);color:var(--txt);font:inherit}
output{min-width:3ch;text-align:right;font-variant-numeric:tabular-nums;color:var(--mut)}
button{padding:9px 14px;border-radius:8px;border:1px solid var(--line);background:var(--bg);color:var(--txt);font:inherit;cursor:pointer}
button:active{background:var(--line)}
button.wide{width:100%;padding:12px;background:var(--card);border-radius:10px}
.seg{display:flex;flex:1;border:1px solid var(--line);border-radius:8px;overflow:hidden}
.seg button{flex:1;border:0;border-radius:0}
.seg button.on{background:var(--acc);color:#fff}
.themes{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:8px;padding:6px 0}
.tw{position:relative;display:flex}
.theme{text-align:left;padding:8px 10px;flex:1}
.del{position:absolute;top:3px;right:3px;padding:0 7px;line-height:1.5;font-size:.9rem;color:var(--mut);background:transparent;border:0}
.del:hover{color:var(--bad)}
.sw{display:flex;gap:3px;margin-top:6px}
.sw i{flex:1;height:10px;border-radius:2px}
.st{font-size:.85rem;color:var(--mut)}
.st.ok{color:var(--ok)}.st.bad{color:var(--bad)}
#st{height:1.2em;text-align:center;color:var(--mut);font-size:.85rem;margin-top:12px}
[hidden]{display:none!important}
</style></head><body><main>
<h1>Horloge</h1>
<p class="sub">Les réglages s'appliquent tout de suite et sont mémorisés.</p>

<section>
 <h2>Thème</h2>
 <div class="themes" id="themes"></div>
 <div class="row"><input type="text" id="themeName" maxlength="16" placeholder="Nom de mon thème"><button id="themeSave">Enregistrer</button></div>
 <p class="hint" id="themeHint">Enregistre les couleurs actuelles comme thème personnel.</p>
</section>

<section>
 <h2>Luminosité</h2>
 <div class="row"><label for="bright">Jour</label><input type="range" id="bright" min="1" max="255"><output id="brightV"></output></div>
 <div class="row"><label for="nightOn">Luminosité de nuit automatique</label><input type="checkbox" id="nightOn"></div>
 <div class="row" id="nightRow"><label for="nightBright">Nuit</label><input type="range" id="nightBright" min="1" max="255"><output id="nightBrightV"></output></div>
 <p class="hint" id="nightHint"></p>
</section>

<section>
 <h2>Panneau du bas</h2>
 <div class="row"><div class="seg" id="mode"><button data-v="0">Météo</button><button data-v="1">Message défilant</button></div></div>
 <div id="msgBox">
  <div class="row"><input type="text" id="msg" maxlength="150" placeholder="Votre message"><button id="msgOk">Afficher</button></div>
  <p class="hint">Les accents sont remplacés par la lettre simple (é → e).</p>
  <div class="row"><label for="msgColor">Couleur</label><input type="color" id="msgColor"></div>
  <div class="row"><label for="speed">Vitesse</label><input type="range" id="speed" min="5" max="80"><output id="speedV"></output></div>
 </div>
</section>

<section>
 <h2>Météo</h2>
 <div class="row"><input type="text" id="city" placeholder="Ville,Pays (ex. LYON,FR)"><button id="cityOk">Valider</button></div>
 <p class="st" id="cityStatus"></p>
</section>

<section>
 <h2>Date</h2>
 <div class="row"><select id="dateFmt">
  <option value="0">22/11/2026</option><option value="1">22/11/26</option>
  <option value="2">22 nov</option><option value="3">2026-11-22</option>
 </select></div>
</section>

<section id="colors"><h2>Couleurs</h2></section>

<section>
 <h2>Mise à jour à distance (MQTT)</h2>
 <p class="st">Identifiant : <b id="mqttId"></b><br>État : <span id="mqttState"></span></p>
 <div class="row"><label for="encrypt">Chiffrer les échanges MQTT</label><input type="checkbox" id="encrypt"></div>
 <p class="hint">Sans chiffrement, le contenu des réglages est lisible par tous sur le broker public (mais personne ne peut les modifier sans la clé).</p>
</section>

<button class="wide" id="reset">Revenir aux réglages d'origine</button>
<div id="st"></div>
</main><script>
const $=id=>document.getElementById(id), st=$('st'), timers={};
function post(url){return fetch(url,{method:'POST'}).then(r=>{st.textContent=r.ok?'Enregistré':'Erreur';return r})
  .catch(()=>{st.textContent='Horloge injoignable'})}
function send(k,v,now){ // pendant qu'on fait glisser : un envoi toutes les 120 ms max par réglage
  clearTimeout(timers[k]);
  const go=()=>post('/api/set?k='+k+'&v='+encodeURIComponent(v));
  if(now)return go(); timers[k]=setTimeout(go,120);
}
function slider(id,key){const s=$(id),o=$(id+'V');s.oninput=()=>{o.textContent=s.value;send(key,s.value)}}
slider('bright','bright');slider('nightBright','nightBr');slider('speed','speed');
$('nightOn').onchange=e=>{$('nightRow').hidden=!e.target.checked;send('nightOn',e.target.checked?1:0,1).then(()=>setTimeout(load,300))};
$('msgOk').onclick=()=>send('msg',$('msg').value,1);
$('msg').onkeydown=e=>{if(e.key==='Enter')$('msgOk').click()};
$('msgColor').oninput=e=>send('msg_c',e.target.value.slice(1));
$('cityOk').onclick=()=>{$('cityStatus').textContent='Recherche…';$('cityStatus').className='st';
  send('city',$('city').value.trim(),1).then(()=>setTimeout(load,4000))};
$('city').onkeydown=e=>{if(e.key==='Enter')$('cityOk').click()};
$('dateFmt').onchange=e=>send('dateFmt',e.target.value,1);
$('encrypt').onchange=e=>send('encrypt',e.target.checked?1:0,1);
$('mode').onclick=e=>{const v=e.target.dataset.v;if(v===undefined)return;setMode(v);send('bottom',v,1)};
function setMode(v){[...$('mode').children].forEach(b=>b.classList.toggle('on',b.dataset.v==v));$('msgBox').hidden=v!=1}
$('themeSave').onclick=()=>{const n=$('themeName').value.trim();if(!n)return;
  send('saveTheme',n,1).then(r=>{if(r&&r.ok){$('themeName').value='';load()}
    else $('themeHint').textContent='Impossible : nom déjà pris par un thème prédéfini, trop long, ou plus de place.'})};
$('themeName').onkeydown=e=>{if(e.key==='Enter')$('themeSave').click()};
$('reset').onclick=()=>post('/api/reset').then(load);

function load(){
  fetch('/api/settings').then(r=>r.json()).then(s=>{
    for(const k of ['bright','nightBright','speed']){$(k).value=s[k];$(k+'V').textContent=s[k]}
    $('nightOn').checked=s.nightOn;$('nightRow').hidden=!s.nightOn;
    $('nightHint').textContent=s.nightOn?('Entre le coucher et le lever du soleil'+(s.nightActive?' — actif en ce moment':'')):'';
    setMode(s.bottom);
    if(document.activeElement!==$('msg'))$('msg').value=s.msg;
    if(document.activeElement!==$('city'))$('city').value=s.city;
    const cs=$('cityStatus');
    if(s.weatherCode==200){cs.textContent='Météo reçue pour '+s.cityName;cs.className='st ok'}
    else if(s.weatherCode==404){cs.textContent='Ville introuvable';cs.className='st bad'}
    else if(s.weatherCode){cs.textContent='Météo indisponible (code '+s.weatherCode+')';cs.className='st bad'}
    else{cs.textContent='';cs.className='st'}
    $('dateFmt').value=s.dateFmt;
    $('mqttId').textContent=s.mqttId;$('mqttState').textContent=s.mqttState;
    $('encrypt').checked=!!s.encrypt;
    // Thèmes
    const th=$('themes');th.innerHTML='';
    const nUser=s.themes.filter(t=>t.user).length;
    $('themeHint').textContent='Enregistre les couleurs actuelles comme thème personnel ('+nUser+'/'+s.userThemeMax+').';
    s.themes.forEach(t=>{const w=document.createElement('div');w.className='tw';
      const b=document.createElement('button');b.className='theme';
      b.textContent=t.name;const sw=document.createElement('div');sw.className='sw';
      t.preview.forEach(c=>{const x=document.createElement('i');x.style.background=c;sw.appendChild(x)});
      b.appendChild(sw);b.onclick=()=>send('theme',t.name,1).then(load);w.appendChild(b);
      if(t.user){const d=document.createElement('button');d.className='del';d.textContent='×';d.title='Supprimer';
        d.onclick=()=>{if(confirm('Supprimer le thème « '+t.name+' » ?'))send('delTheme',t.name,1).then(load)};w.appendChild(d)}
      th.appendChild(w)});
    // Couleurs (la couleur du message est réglée dans sa section)
    const box=$('colors');box.querySelectorAll('.row').forEach(e=>e.remove());
    s.colors.forEach(c=>{
      if(c.key==='msg'){$('msgColor').value=c.value;return}
      const row=document.createElement('div');row.className='row';
      row.innerHTML='<label></label><input type="color">';
      row.querySelector('label').textContent=c.label;
      const i=row.querySelector('input');i.value=c.value;i.oninput=()=>send(c.key+'_c',i.value.slice(1));
      box.appendChild(row);
    });
  }).catch(()=>{st.textContent='Horloge injoignable'});
}
load();
</script></body></html>)HTML";

static String hexColor(uint32_t c) {
	char hex[8];
	snprintf(hex, sizeof(hex), "#%06lX", (unsigned long)c);
	return hex;
}

// Etat complet (réglages + infos) : servi à la page web et publié sur MQTT
void buildSettingsJson(JsonDocument& doc) {
	doc["bright"]      = settings.brightness;
	doc["nightOn"]     = settings.nightOn;
	doc["nightBright"] = settings.nightBright;
	doc["nightActive"] = nightActive;
	doc["bottom"]      = settings.bottomMode;
	doc["msg"]         = settings.message;
	doc["speed"]       = settings.scrollSpeed;
	doc["city"]        = settings.city;
	doc["cityName"]    = weather.cityName;
	doc["weatherCode"] = weatherHttpCode;
	if (weatherOk) { // météo actuelle (aperçu de la télécommande)
		JsonObject w = doc["weather"].to<JsonObject>();
		w["temp"] = (int)lroundf(weather.temperature);
		w["hum"]  = (int)lroundf(weather.humidity);
		w["wind"] = (int)lroundf(weather.windspeed * 3.6f); // km/h
		w["icon"] = weather.icon;
		char hm[6];
		struct tm lt;
		localtime_r(&weather.sunrise, &lt); snprintf(hm, sizeof(hm), "%02d:%02d", lt.tm_hour, lt.tm_min); w["rise"] = hm;
		localtime_r(&weather.sunset, &lt);  snprintf(hm, sizeof(hm), "%02d:%02d", lt.tm_hour, lt.tm_min); w["set"] = hm;
	}
	doc["dateFmt"]     = settings.dateFmt;
	doc["encrypt"]     = settings.encrypt;
	doc["uiPrefs"]     = settings.uiPrefs;
	doc["mqttId"]      = mqttDeviceId();
	doc["mqttState"]   = !mqttEnabled() ? "désactivé (clé non configurée)" : mqttConnected() ? "connecté" : "non connecté";
	JsonArray arr = doc["colors"].to<JsonArray>();
	for (int i = 0; i < C_COUNT; i++) {
		JsonObject o = arr.add<JsonObject>();
		o["key"] = colorDefs[i].key;
		o["label"] = colorDefs[i].label;
		o["value"] = hexColor(settings.color[i]);
	}
	JsonArray th = doc["themes"].to<JsonArray>();
	for (int i = 0; i < themeCount; i++) {
		JsonObject o = th.add<JsonObject>();
		o["name"] = themes[i].name;
		JsonArray p = o["preview"].to<JsonArray>(); // aperçu : heure, barres, jour, température, humidité, lever du soleil
		for (ColorId c : { C_HOUR, C_BARS, C_DAY, C_TEMP, C_HUM, C_SUNRISE }) p.add(hexColor(themes[i].color[c]));
	}
	for (int s = 0; s < USER_THEME_MAX; s++) { // thèmes personnels
		if (!userThemes[s].name[0]) continue;
		JsonObject o = th.add<JsonObject>();
		o["name"] = userThemes[s].name;
		o["user"] = true;
		JsonArray p = o["preview"].to<JsonArray>();
		for (ColorId c : { C_HOUR, C_BARS, C_DAY, C_TEMP, C_HUM, C_SUNRISE }) p.add(hexColor(userThemes[s].color[c]));
	}
	doc["userThemeMax"] = USER_THEME_MAX;
}

static void handleSettings() {
	JsonDocument doc;
	buildSettingsJson(doc);
	String out;
	serializeJson(doc, out);
	server.send(200, "application/json", out);
}

static void handleSet() {
	if (settingsApplyKey(server.arg("k"), server.arg("v"))) server.send(200, "text/plain", "OK");
	else server.send(400, "text/plain", "Parametre invalide");
}

static void handleTheme() {
	int i = server.arg("i").toInt();
	if (i < 0 || i >= themeCount) { server.send(400, "text/plain", "Theme inconnu"); return; }
	settingsApplyTheme(i);
	server.send(200, "text/plain", "OK");
}

static void handleReset() {
	String oldCity = settings.city;
	settingsReset();
	if (settings.city != oldCity) weatherRefresh = true;
	server.send(200, "text/plain", "OK");
}

// N'accepte que les clients du Wi-Fi de réglage (jamais ceux du réseau public)
static bool allowed() {
	if (!apActive || server.client().localIP() != WiFi.softAPIP()) {
		server.send(403, "text/plain", "Interdit");
		return false;
	}
	apLastActivity = millis();
	return true;
}

static void handleRoot() {
	if (!allowed()) return;
	server.send_P(200, "text/html; charset=utf-8", PAGE);
}

// Portail captif : toute autre adresse (tests de connexion des téléphones) renvoie vers la page
static void handleCaptive() {
	if (!allowed()) return;
	server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
	server.send(302, "text/plain", "");
}

void webBegin() {
	server.on("/",             HTTP_GET,  handleRoot);
	server.on("/api/settings", HTTP_GET,  [] { if (allowed()) handleSettings(); });
	server.on("/api/set",      HTTP_POST, [] { if (allowed()) handleSet(); });
	server.on("/api/theme",    HTTP_POST, [] { if (allowed()) handleTheme(); });
	server.on("/api/reset",    HTTP_POST, [] { if (allowed()) handleReset(); });
	server.onNotFound(handleCaptive);
}

static void apOpen() {
	// Mot de passe aléatoire sans caractères ambigus (0/O, 1/l/I)
	static const char alphabet[] = "23456789abcdefghjkmnpqrstuvwxyz";
	for (int i = 0; i < 8; i++) apPass[i] = alphabet[esp_random() % (sizeof(alphabet) - 1)];
	apPass[8] = 0;

	WiFi.mode(WIFI_AP_STA); // l'horloge reste connectée au réseau public pour la météo
	if (!WiFi.softAP(AP_SSID, apPass, 0, 0, AP_MAX_CLIENTS)) {
		Serial.println("Impossible d'ouvrir le Wi-Fi de réglage");
		WiFi.mode(WIFI_STA);
		return;
	}
	dns.start(53, "*", WiFi.softAPIP());
	server.begin();
	apActive = true;
	apHadClient = false;
	apEmptySince = 0;
	apOpenedAt = millis();
	apLastActivity = millis();
	Serial.printf("Wi-Fi de réglage ouvert : %s / %s -> http://%s\n", AP_SSID, apPass, WiFi.softAPIP().toString().c_str());
}

static void apClose() {
	server.stop();
	dns.stop();
	WiFi.softAPdisconnect(true);
	WiFi.mode(WIFI_STA);
	apActive = false;
	memset(apPass, 0, sizeof(apPass));
	Serial.println("Wi-Fi de réglage fermé");
}

void webApToggle() {
	if (apActive) apClose(); else apOpen();
}

bool webApActive() { return apActive; }
const char* webApSsid() { return AP_SSID; }
const char* webApPassword() { return apPass; }

void webLoop() {
	if (apActive) {
		dns.processNextRequest();
		server.handleClient();
		// Fermeture dès que le téléphone se déconnecte (après un court délai de grâce :
		// certains téléphones se déconnectent une fraction de seconde en testant le portail captif)
		int stations = WiFi.softAPgetStationNum();
		if (stations > 0) {
			apHadClient = true;
			apEmptySince = 0;
		} else if (apHadClient) {
			if (!apEmptySince) apEmptySince = millis() | 1;
			else if (millis() - apEmptySince > AP_DISCONNECT_GRACE_MS) {
				Serial.println("Téléphone déconnecté");
				apClose();
				return;
			}
		}
		// Sinon fermeture automatique s'il n'y a plus d'activité (personne connecté et rien demandé)
		if (millis() - apOpenedAt > AP_MAX_OPEN_MS) apClose(); // durée max, même si quelqu'un reste connecté
		else if (millis() - apLastActivity > AP_IDLE_TIMEOUT_MS) {
			if (WiFi.softAPgetStationNum() > 0) apLastActivity = millis(); // quelqu'un est encore connecté
			else apClose();
		}
	}
	settingsLoop();
}
