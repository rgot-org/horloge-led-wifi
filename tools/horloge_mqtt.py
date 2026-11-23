#!/usr/bin/env python3
"""
Envoie des réglages signés à l'horloge par MQTT.

Installation :  pip install paho-mqtt

Exemples :
  python horloge_mqtt.py --id a1b2c3d4e5f6 msg="Bonne journee" bottom=message
  python horloge_mqtt.py --id a1b2c3d4e5f6 theme=Nuit bright=40
  python horloge_mqtt.py --id a1b2c3d4e5f6 city=LYON,FR hour_c=FF8800

La clé secrète (la même que MQTT_SECRET dans config.h) est lue dans la variable
d'environnement HORLOGE_SECRET, ou passée avec --secret.

Réglages possibles :
  bright=1..255  nightOn=0|1  nightBr=1..255  bottom=weather|message  speed=5..80
  dateFmt=0..3  msg="texte"  city=VILLE,PAYS  theme=<nom ou numéro>
  saveTheme=<nom> (couleurs actuelles -> thème personnel)  delTheme=<nom>
  encrypt=0|1  (chiffrement des échanges ; ajouter --encrypt pour chiffrer cette commande)
  <couleur>_c=RRGGBB  avec <couleur> = hour bars day sunday date temp tempneg hum wind sunrise sunset msg
"""
import argparse
import hashlib
import hmac
import json
import os
import ssl
import sys
import threading
import time

try:
    import paho.mqtt.client as mqtt
except ImportError:
    sys.exit("Il manque paho-mqtt :  pip install paho-mqtt")


def build_payload(secret: str, settings: dict, encrypt: bool = False) -> bytes:
    body = json.dumps({"ts": int(time.time() * 1000), "set": settings},  # ts en millisecondes
                      separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    if encrypt:
        return seal(secret, body).encode("ascii")
    sig = hmac.new(secret.encode("utf-8"), body, hashlib.sha256).hexdigest()
    return body + b"." + sig.encode("ascii")


# ---------- Chiffrement (AES-256-CTR + HMAC-SHA256, identique à l'horloge) ----------
# Format : E1.<IV base64>.<données chiffrées base64>.<HMAC hex>   clé AES = HMAC(clé, "horloge-enc")
def _aes(secret: str):
    try:
        from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    except ImportError:
        sys.exit("Le chiffrement demande la bibliothèque cryptography :  pip install cryptography")
    key = hmac.new(secret.encode("utf-8"), b"horloge-enc", hashlib.sha256).digest()
    return lambda iv: Cipher(algorithms.AES(key), modes.CTR(iv))


def seal(secret: str, plain: bytes) -> str:
    import base64
    iv = os.urandom(16)
    enc = _aes(secret)(iv).encryptor()
    body = "E1." + base64.b64encode(iv).decode() + "." + base64.b64encode(enc.update(plain) + enc.finalize()).decode()
    return body + "." + hmac.new(secret.encode("utf-8"), body.encode("ascii"), hashlib.sha256).hexdigest()


def unseal(secret: str, msg: str) -> str:
    import base64
    body, sig = msg.rsplit(".", 1)
    expected = hmac.new(secret.encode("utf-8"), body.encode("ascii"), hashlib.sha256).hexdigest()
    if not hmac.compare_digest(sig.lower(), expected):
        raise ValueError("signature invalide")
    _, iv, ct = body.split(".")
    dec = _aes(secret)(base64.b64decode(iv)).decryptor()
    return (dec.update(base64.b64decode(ct)) + dec.finalize()).decode("utf-8")


def main():
    ap = argparse.ArgumentParser(description="Réglages signés pour l'horloge, par MQTT")
    ap.add_argument("--id", required=True, help="identifiant de l'horloge (affiché au démarrage)")
    ap.add_argument("--secret", default=os.environ.get("HORLOGE_SECRET"), help="clé secrète (sinon HORLOGE_SECRET)")
    ap.add_argument("--broker", default="broker.hivemq.com")
    ap.add_argument("--port", type=int, default=8883)
    ap.add_argument("--no-tls", action="store_true", help="port 1883 sans chiffrement")
    ap.add_argument("--encrypt", action="store_true", help="chiffre la commande (pip install cryptography)")
    ap.add_argument("reglages", nargs="+", help="clé=valeur")
    args = ap.parse_args()

    if not args.secret:
        sys.exit("Clé secrète manquante : --secret ou variable HORLOGE_SECRET")
    settings = {}
    for item in args.reglages:
        if "=" not in item:
            sys.exit(f"Réglage invalide (attendu clé=valeur) : {item}")
        k, v = item.split("=", 1)
        settings[k] = v

    topic = f"horloge/{args.id}"
    done = threading.Event()

    def on_connect(client, userdata, flags, rc, *extra):
        client.subscribe(f"{topic}/ack")
        client.publish(f"{topic}/cmd", build_payload(args.secret, settings, args.encrypt))
        print(("Commande chiffrée envoyée" if args.encrypt else "Commande envoyée") + ", attente de la réponse de l'horloge...")

    def on_message(client, userdata, msg):
        txt = msg.payload.decode("utf-8")
        if txt.startswith("E1."):  # réponse chiffrée : signature vérifiée puis déchiffrement
            try:
                txt = unseal(args.secret, txt)
            except ValueError as e:
                print(f"Réponse rejetée : {e}")
                return
        r = json.loads(txt)
        print(("OK : " if r.get("ok") else "REFUSÉ : ") + r.get("msg", ""))
        done.set()

    try:  # paho-mqtt 2.x
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    except AttributeError:  # paho-mqtt 1.x
        client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    port = 1883 if args.no_tls else args.port
    if not args.no_tls:
        client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
    client.connect(args.broker, port, keepalive=30)
    client.loop_start()
    if not done.wait(15):
        print("Pas de réponse de l'horloge en 15 s (hors ligne, mauvais identifiant ou mauvaise clé ?)")
    client.loop_stop()
    client.disconnect()


if __name__ == "__main__":
    main()
