# gestor.py
import json
import random
import threading
import time
from typing import Dict, Tuple

import paho.mqtt.client as mqtt

BROKER = "broker.hivemq.com"
PORT = 1883

lock = threading.Lock()

# Config por equipe (recebida do manager em "equipeNN")
# configs["equipe01"] = {"cor":"#FF0000","qtd":3,"round_id":"..."}
configs: Dict[str, Dict[str, object]] = {}

# Segredos por EQUIPE (dona do cofre). Cada equipe corresponde ao cofreNN.
# secrets["equipe01"] = {"etapas":{"etapa01":"1234",...}, "round_id":"..."}
secrets: Dict[str, Dict[str, object]] = {}

# ---------- Utils ----------
def gen_secret_4distinct() -> str:
    return "".join(random.sample("0123456789", 4))

def etapa_name_from_any(v: str) -> str:
    s = "".join(ch for ch in str(v) if ch.isdigit())
    try:
        n = max(1, min(100, int(s)))
    except ValueError:
        n = 1
    return f"etapa{n:02d}"

def number_from_str(s: str) -> int:
    d = "".join(ch for ch in s if ch.isdigit())
    try:
        return int(d)
    except ValueError:
        return 0

def team_from_safe(safe: str) -> str:
    n = number_from_str(safe)
    return f"equipe{n:02d}" if n > 0 else ""

def safe_from_team(team: str) -> str:
    n = number_from_str(team)
    return f"cofre{n:02d}" if n > 0 else ""

def eval_feedback(secret: str, attempt: str) -> Tuple[int, int]:
    otimos = sum(1 for i in range(4) if attempt[i] == secret[i])
    bons = sum(1 for ch in attempt if ch in secret) - otimos
    return bons, otimos

def fmt_bons_otimos(b: int, o: int) -> str:
    btxt = "1 Bom" if b == 1 else f"{b} Bons"
    otxt = "1 Ótimo" if o == 1 else f"{o} Ótimos"
    return f"{btxt} e {otxt}"

def ensure_secrets_for_team(team: str, qtd: int, round_id: str | None, force_reset: bool = False):
    """Gera/ajusta secrets da equipe. Se force_reset=True ou round_id mudou, regenera tudo."""
    qtd = max(1, min(100, int(qtd)))
    cur = secrets.get(team)

    if cur is None or force_reset:
        etapas = {f"etapa{i:02d}": gen_secret_4distinct() for i in range(1, qtd + 1)}
        secrets[team] = {"etapas": etapas, "round_id": round_id}
        return True  # regenerated

    cur_round = cur.get("round_id")
    etapas: Dict[str, str] = cur["etapas"]  # type: ignore

    if round_id is not None and round_id != cur_round:
        etapas = {f"etapa{i:02d}": gen_secret_4distinct() for i in range(1, qtd + 1)}
        secrets[team] = {"etapas": etapas, "round_id": round_id}
        return True  # regenerated

    # mesma rodada: apenas ajusta quantidade
    existing = len(etapas)
    changed = False
    if qtd > existing:
        for i in range(existing + 1, qtd + 1):
            etapas[f"etapa{i:02d}"] = gen_secret_4distinct()
            changed = True
    elif qtd < existing:
        for i in range(qtd + 1, existing + 1):
            etapas.pop(f"etapa{i:02d}", None)
            changed = True

    cur["etapas"] = etapas
    if round_id is not None:
        cur["round_id"] = round_id
    return changed

# ---------- MQTT callbacks ----------
def on_connect(client: mqtt.Client, userdata, flags, rc):
    print(f"[GESTOR] Conectado rc={rc}")

    # manager publica configs em "equipeNN"
    for i in range(1, 51):
        client.subscribe(f"equipe{i:02d}")

    # app envia tentativas em "equipeNN/enviado"
    for i in range(1, 51):
        client.subscribe(f"equipe{i:02d}/enviado")

    print("[GESTOR] Assinado: equipe01..50 e equipe01..50/enviado")

def publish_team_passwords(client: mqtt.Client, team: str):
    """Publica em equipeNN/senhaCorreta as senhas (retain)."""
    et = secrets.get(team, {}).get("etapas", {})  # type: ignore
    # ordena por etapa
    ordenadas = [et[f"etapa{i:02d}"] for i in range(1, len(et)+1) if f"etapa{i:02d}" in et]
    topic = f"{team}/senhaCorreta"
    payload = json.dumps({"senhas": ordenadas})
    client.publish(topic, payload, qos=0, retain=True)
    print(f"[GESTOR] Publicado (retain) {topic}: {payload}")

def on_message(client: mqtt.Client, userdata, msg: mqtt.MQTTMessage):
    topic = msg.topic.strip()
    payload_raw = msg.payload.decode(errors="ignore").strip()
    if not payload_raw:
        return

    # 1) CONFIG do manager em "equipeNN"
    if topic.startswith("equipe") and "/" not in topic:
        try:
            data = json.loads(payload_raw)
        except Exception:
            return

        if not (isinstance(data, dict) and "qtd_senha" in data and "cor" in data):
            return

        qtd = int(data.get("qtd_senha", 1))
        cor = str(data.get("cor", "#FFFFFF")).strip()
        if not cor.startswith("#"):
            cor = "#" + cor
        round_id = str(data.get("round_id")) if "round_id" in data else None
        reset = bool(data.get("reset", False))

        team = topic  # equipeNN
        with lock:
            cfg = configs.get(team, {})
            cfg["cor"] = cor
            cfg["qtd"] = max(1, min(100, qtd))
            if round_id is not None:
                cfg["round_id"] = round_id
            configs[team] = cfg

            if reset:
                secrets.pop(team, None)

            changed = ensure_secrets_for_team(team, cfg["qtd"], cfg.get("round_id") if "round_id" in cfg else None, force_reset=reset)
            print(f"[CONFIG] {team}: cor={cfg['cor']} qtd={cfg['qtd']} round_id={cfg.get('round_id')}")

            # sempre publica as senhas atuais (retain) após processar a config
            publish_team_passwords(client, team)
        return

    # 2) TENTATIVA do app em "equipeYY/enviado"
    if topic.endswith("/enviado") and topic.startswith("equipe"):
        atacante = topic.split("/")[0]  # equipeYY
        resposta_topic = f"{atacante}/resposta"

        try:
            data = json.loads(payload_raw)
        except Exception:
            client.publish(resposta_topic, json.dumps({"error": "Payload não-JSON"}), retain=False)
            return

        if not (isinstance(data, dict) and "cofre" in data and "etapa" in data and "senha" in data):
            client.publish(resposta_topic, json.dumps({"error": "JSON incompleto"}), retain=False)
            return

        cofre = str(data["cofre"]).strip()           # ex: cofre02
        etapa_in = etapa_name_from_any(data["etapa"])
        attempt = str(data["senha"]).strip()

        # valida tentativa
        if len(attempt) != 4 or (not attempt.isdigit()) or len(set(attempt)) != 4:
            client.publish(resposta_topic, json.dumps({"error": "Formato inválido (4 dígitos distintos)."}), retain=False)
            print(f"[{topic}] {atacante} {etapa_in} tentativa inválida: {attempt}")
            return

        dono = team_from_safe(cofre)  # equipeNN dona do cofre
        if not dono:
            client.publish(resposta_topic, json.dumps({"error": "Cofre inválido."}), retain=False)
            return

        # evita auto-invasão (opcional – app já esconde)
        if dono == atacante:
            client.publish(resposta_topic, json.dumps({"error": "Não é permitido invadir o próprio cofre."}), retain=False)
            return

        with lock:
            cfg = configs.get(dono, {"qtd": 1})
            round_id = str(cfg.get("round_id")) if "round_id" in cfg else None
            ensure_secrets_for_team(dono, cfg.get("qtd", 1), round_id)

            etapa_map: Dict[str, str] = secrets[dono]["etapas"]  # type: ignore
            secret = etapa_map.get(etapa_in)
            if not secret:
                client.publish(resposta_topic, json.dumps({"error": f"Etapa inválida para {cofre}."}), retain=False)
                print(f"[{cofre}] {atacante} solicitou {etapa_in} inexistente.")
                return

            if attempt == secret:
                ok = {"status": "ok", "cofre": cofre, "etapa": etapa_in}
                client.publish(resposta_topic, json.dumps(ok), retain=False)
                print(f"[{cofre}] {atacante} ACERTOU {etapa_in}: {attempt}")
            else:
                b, o = eval_feedback(secret, attempt)
                hint = {"status": "hint", "cofre": cofre, "etapa": etapa_in, "bons": b, "otimos": o,
                        "msg": fmt_bons_otimos(b, o)}
                client.publish(resposta_topic, json.dumps(hint), retain=False)
                print(f"[{cofre}] {atacante} {etapa_in} tentativa={attempt} -> {hint['msg']}")
        return

# ---------- Runner ----------
def main():
    random.seed()
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, 60)

    thr = threading.Thread(target=client.loop_forever, daemon=True)
    thr.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[GESTOR] Encerrando...")
        client.disconnect()

if __name__ == "__main__":
    main()
