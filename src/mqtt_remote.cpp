#include "mqtt_remote.h"
#include "config.h"
#include "settings.h"
#include "weather.h"
#include "webui.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include <esp_random.h>
#include <memory>

#if MQTT_TLS
static WiFiClientSecure net;
#else
static WiFiClient net;
#endif
static PubSubClient mqtt(net);
static String deviceId, topicCmd, topicAck, topicStatus, topicState;
static unsigned long lastAttempt = 0;
static uint64_t lastTs = 0; // horodatage (ms) de la dernière commande acceptée (anti-rejeu)
static uint32_t publishedSettings = 0xFFFFFFFF, publishedWeather = 0xFFFFFFFF;
static unsigned long changedAt = 0, lastOnline = 0;

bool mqttEnabled() { return strcmp(MQTT_SECRET, "CHANGEZ_MOI") != 0 && strlen(MQTT_SECRET) >= 16; }
bool mqttConnected() { return mqtt.connected(); }
String mqttDeviceId() { return deviceId; }

// HMAC-SHA256(MQTT_SECRET, data) en hexadécimal
static String hmacHex(const uint8_t* data, size_t len) {
	uint8_t mac[32];
	mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
	                (const uint8_t*)MQTT_SECRET, strlen(MQTT_SECRET), data, len, mac);
	char hex[65];
	for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", mac[i]);
	return String(hex);
}

// Comparaison en temps constant (ne révèle pas, par le temps de réponse, combien de caractères sont justes)
static bool sameSig(const String& a, const String& b) {
	if (a.length() != b.length()) return false;
	uint8_t diff = 0;
	for (unsigned i = 0; i < a.length(); i++) diff |= a[i] ^ b[i];
	return diff == 0;
}

// ---------- Chiffrement des messages (AES-256-CTR + HMAC-SHA256, « chiffrer puis signer ») ----------
// Format d'un message chiffré :  E1.<IV base64>.<données chiffrées base64>.<HMAC hex>
//   IV      = 16 octets aléatoires, nouveaux à chaque message (compteur CTR 128 bits)
//   clé AES = HMAC-SHA256(MQTT_SECRET, "horloge-enc")   (dérivée de la clé partagée)
//   HMAC    = HMAC-SHA256(MQTT_SECRET, "E1.<IV>.<données>")
static uint8_t encKey[32];

static String b64enc(const uint8_t* data, size_t len) {
	size_t olen = 0;
	mbedtls_base64_encode(nullptr, 0, &olen, data, len);
	std::unique_ptr<uint8_t[]> buf(new uint8_t[olen + 1]);
	mbedtls_base64_encode(buf.get(), olen + 1, &olen, data, len);
	return String((const char*)buf.get(), olen);
}

static bool b64dec(const char* s, size_t slen, std::unique_ptr<uint8_t[]>& out, size_t& olen) {
	olen = 0;
	mbedtls_base64_decode(nullptr, 0, &olen, (const uint8_t*)s, slen);
	out.reset(new uint8_t[olen + 1]);
	return mbedtls_base64_decode(out.get(), olen + 1, &olen, (const uint8_t*)s, slen) == 0;
}

static void aesCtr(const uint8_t iv[16], const uint8_t* in, uint8_t* out, size_t len) {
	mbedtls_aes_context aes;
	mbedtls_aes_init(&aes);
	mbedtls_aes_setkey_enc(&aes, encKey, 256);
	uint8_t nonce[16], stream[16];
	memcpy(nonce, iv, 16);
	size_t off = 0;
	mbedtls_aes_crypt_ctr(&aes, len, &off, nonce, stream, in, out);
	mbedtls_aes_free(&aes);
}

// Chiffre et signe un texte
static String seal(const String& plain) {
	uint8_t iv[16];
	esp_fill_random(iv, sizeof(iv));
	std::unique_ptr<uint8_t[]> ct(new uint8_t[plain.length() + 1]);
	aesCtr(iv, (const uint8_t*)plain.c_str(), ct.get(), plain.length());
	String body = "E1." + b64enc(iv, 16) + "." + b64enc(ct.get(), plain.length());
	return body + "." + hmacHex((const uint8_t*)body.c_str(), body.length());
}

// Vérifie la signature puis déchiffre un message "E1...". Retourne false si invalide.
static bool unseal(const uint8_t* p, unsigned len, String& plain, const char*& err) {
	// Points séparateurs : après "E1", après l'IV, avant la signature (le base64 et l'hexa n'ont pas de point)
	const int d1 = 2;
	int d2 = -1, d3 = -1;
	for (unsigned i = 3; i < len; i++) if (p[i] == '.') { if (d2 < 0) d2 = i; d3 = i; }
	if (d2 < 0 || d3 <= d2 || len - d3 - 1 != 64) { err = "format invalide"; return false; }
	String sig = String((const char*)p + d3 + 1).substring(0, 64);
	sig.toLowerCase();
	if (!sameSig(sig, hmacHex(p, d3))) { err = "signature invalide"; return false; }
	std::unique_ptr<uint8_t[]> iv, ct;
	size_t ivLen, ctLen;
	if (!b64dec((const char*)p + d1 + 1, d2 - d1 - 1, iv, ivLen) || ivLen != 16 ||
	    !b64dec((const char*)p + d2 + 1, d3 - d2 - 1, ct, ctLen)) { err = "format invalide"; return false; }
	std::unique_ptr<uint8_t[]> pt(new uint8_t[ctLen + 1]);
	aesCtr(iv.get(), ct.get(), pt.get(), ctLen);
	plain = String((const char*)pt.get(), ctLen);
	return true;
}

static void ack(uint64_t ts, bool ok, const char* msg, bool encrypted) {
	JsonDocument d;
	d["ts"] = ts;
	d["ok"] = ok;
	d["msg"] = msg;
	String out;
	serializeJson(d, out);
	if (encrypted) out = seal(out); // réponse dans le même format que la commande
	mqtt.publish(topicAck.c_str(), out.c_str());
	Serial.printf("MQTT : %s (%s)\n", ok ? "commande acceptée" : "commande refusée", msg);
}

// Publie l'état complet (retenu : l'IHM le reçoit dès qu'elle se connecte), chiffré si demandé
static void publishState() {
	JsonDocument doc;
	buildSettingsJson(doc);
	doc["mqttState"] = "connecté";
	String out;
	serializeJson(doc, out);
	if (settings.encrypt) out = seal(out);
	if (!mqtt.publish(topicState.c_str(), out.c_str(), true))
		Serial.printf("MQTT : état trop gros (%d octets)\n", out.length());
	publishedSettings = settingsVersion;
	publishedWeather = weatherVersion;
}

static void onMessage(char* topic, uint8_t* payload, unsigned int len) {
	// Deux formats acceptés, tous deux signés :
	//   chiffré : E1.<IV>.<données>.<signature>     en clair : <json>.<signature>
	bool encrypted = len > 3 && memcmp(payload, "E1.", 3) == 0;
	String body;
	if (encrypted) {
		const char* err;
		if (!unseal(payload, len, body, err)) { ack(0, false, err, true); return; }
	} else {
		int dot = -1;
		for (int i = len - 1; i >= 0; i--) if (payload[i] == '.') { dot = i; break; }
		if (dot < 0 || len - dot - 1 != 64) { ack(0, false, "format invalide", false); return; }
		String sig = String((const char*)payload + dot + 1).substring(0, 64);
		sig.toLowerCase();
		if (!sameSig(sig, hmacHex(payload, dot))) { ack(0, false, "signature invalide", false); return; }
		body = String((const char*)payload, dot);
	}

	// Signature correcte : on peut lire le contenu
	JsonDocument doc;
	if (deserializeJson(doc, body)) { ack(0, false, "JSON invalide", encrypted); return; }
	uint64_t ts = doc["ts"] | (uint64_t)0; // millisecondes
	struct timeval tv;
	gettimeofday(&tv, NULL);
	if (tv.tv_sec < 1700000000) { ack(ts, false, "horloge pas encore a l'heure", encrypted); return; }
	int64_t now = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
	if (ts <= lastTs) { ack(ts, false, "commande deja recue (rejeu)", encrypted); return; }
	if (now - (int64_t)ts > MQTT_MAX_AGE_S * 1000LL || (int64_t)ts - now > 60000) {
		ack(ts, false, "horodatage trop ancien ou dans le futur (verifier l'heure du PC)", encrypted);
		return;
	}

	// Anti-rejeu : on mémorise l'horodatage (aussi en flash, pour survivre à un redémarrage)
	lastTs = ts;
	Preferences p;
	p.begin("mqtt", false);
	p.putULong64("lastTsMs", lastTs);
	p.end();

	JsonObject set = doc["set"];
	int okCount = 0, badCount = 0;
	for (JsonPair kv : set) {
		String v = kv.value().is<const char*>() ? String(kv.value().as<const char*>()) : kv.value().as<String>();
		if (settingsApplyKey(kv.key().c_str(), v)) okCount++;
		else { badCount++; Serial.printf("MQTT : réglage refusé %s=%s\n", kv.key().c_str(), v.c_str()); }
	}
	char msg[48];
	snprintf(msg, sizeof(msg), "%d reglage(s) applique(s), %d refuse(s)", okCount, badCount);
	ack(ts, badCount == 0, msg, encrypted);
}

void mqttBegin() {
	if (strlen(MQTT_DEVICE_ID)) deviceId = MQTT_DEVICE_ID;
	else {
		uint8_t m[6];
		WiFi.macAddress(m);
		char id[13];
		snprintf(id, sizeof(id), "%02x%02x%02x%02x%02x%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
		deviceId = id;
	}
	String base = "horloge/" + deviceId;
	topicCmd    = base + "/cmd";
	topicAck    = base + "/ack";
	topicStatus = base + "/status";
	topicState  = base + "/state";

	Serial.printf("MQTT : identifiant de l'horloge = %s\n", deviceId.c_str());
	if (!mqttEnabled()) {
		Serial.println("MQTT désactivé : MQTT_SECRET doit être changé et faire au moins 16 caractères (config.h)");
		return;
	}
	// Clé de chiffrement dérivée de la clé partagée (la clé de signature reste MQTT_SECRET)
	mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const uint8_t*)MQTT_SECRET, strlen(MQTT_SECRET),
	                (const uint8_t*)"horloge-enc", 11, encKey);

	Preferences p;
	p.begin("mqtt", false); // lecture-écriture : crée l'espace au premier démarrage (sinon erreur NOT_FOUND)
	lastTs = p.getULong64("lastTsMs", 0);
	p.end();

#if MQTT_TLS
	net.setInsecure(); // chiffrement sans vérifier le broker : l'authenticité est garantie par la signature
#endif
	mqtt.setServer(MQTT_BROKER, MQTT_PORT);
	mqtt.setBufferSize(4096); // l'état complet fait ~2 ko, ~2,8 ko une fois chiffré (base64)
	mqtt.setCallback(onMessage);
	Serial.printf("MQTT : identifiant %s, commandes sur %s\n", deviceId.c_str(), topicCmd.c_str());
}

void mqttLoop() {
	if (!mqttEnabled() || WiFi.status() != WL_CONNECTED) return;
	if (mqtt.connected()) {
		mqtt.loop();
		// Republie "online" régulièrement : une ancienne connexion coupée brutalement (redémarrage)
		// peut faire publier son "offline" par le broker après notre "online".
		if (millis() - lastOnline > 60000) { mqtt.publish(topicStatus.c_str(), "online", true); lastOnline = millis(); }
		// Etat modifié (réglages ou météo) : publication 1 s après le dernier changement
		if (settingsVersion != publishedSettings || weatherVersion != publishedWeather) {
			if (!changedAt) changedAt = millis() | 1;
			else if (millis() - changedAt > 1000) { changedAt = 0; publishState(); }
		}
		return;
	}
	// Reconnexion toutes les 30 s (la connexion TLS bloque 1 à 2 s)
	if (lastAttempt && millis() - lastAttempt < 30000) return;
	lastAttempt = millis();
	String clientId = "horloge-" + deviceId + "-" + String(esp_random() & 0xFFFF, HEX);
	if (mqtt.connect(clientId.c_str(), nullptr, nullptr, topicStatus.c_str(), 0, true, "offline")) {
		mqtt.publish(topicStatus.c_str(), "online", true);
		lastOnline = millis();
		mqtt.subscribe(topicCmd.c_str());
		publishState();
		Serial.println("MQTT connecté");
	} else {
		Serial.printf("MQTT : connexion impossible (code %d)\n", mqtt.state());
	}
}

// Coupe MQTT le temps d'une autre connexion TLS (météo) : la RAM ne permet pas deux
// connexions chiffrées en même temps. mqttLoop() se reconnecte dès le prochain passage.
void mqttSuspend() {
#if MQTT_TLS
	if (!mqtt.connected()) return;
	mqtt.disconnect();
	net.stop();
	lastAttempt = 0; // reconnexion immédiate ensuite
#endif
}
