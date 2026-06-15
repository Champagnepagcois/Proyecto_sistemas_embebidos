# MPU - Arduino UNO Q
# Lee datos cacheados del MCU mediante Bridge/RPC
# y publica mensajes MQTT hacia Mosquitto.

from arduino.app_utils import *
import json
import os
import time
from datetime import datetime, timezone

import paho.mqtt.client as mqtt


# =====================================================
# Configuración general
# =====================================================

DEVICE_ID = os.getenv("DEVICE_ID", "unoq-cadena-frio-01")

# Si Mosquitto está corriendo en la misma UNO Q, usa localhost.
# Si Mosquitto está en tu laptop/PC, cambia esto por la IP de tu PC.
MQTT_BROKER = os.getenv("MQTT_BROKER", "172.17.0.1")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))

MQTT_USERNAME = os.getenv("MQTT_USERNAME", "")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")

BASE_TOPIC = os.getenv(
    "MQTT_BASE_TOPIC",
    "escom/iot/equipoX/cadena_frio"
)

TOPIC_SENSORES = f"{BASE_TOPIC}/sensores"
TOPIC_ESTADO = f"{BASE_TOPIC}/estado"
TOPIC_ALERTAS = f"{BASE_TOPIC}/alertas"

# Método RPC expuesto por el sketch del MCU
RPC_METHOD = "get_reading"

# Frecuencia de publicación
PUBLISH_INTERVAL_SECONDS = 2

# Límite de temperatura para cadena de frío
TEMP_MAX_C = 30.0


# =====================================================
# Estado global MQTT
# =====================================================

mqtt_connected = False
mqtt_client = None


# =====================================================
# Utilidades
# =====================================================

def utc_now_iso():
    return datetime.now(timezone.utc).isoformat()


def determine_state(mcu_reading):
    """
    Determina el estado general del sistema a partir de la lectura del MCU.
    """
    data = mcu_reading.get("data", {})
    valid = mcu_reading.get("valid", {})

    ds_ok = valid.get("ds18b20", False)
    bmp_ok = valid.get("bmp280", False)
    mc38_ok = valid.get("mc38", False)

    if not ds_ok or not bmp_ok or not mc38_ok:
        return "ERROR_SENSOR"

    temp_producto = data.get("temperatura_producto_c")
    puerta = data.get("puerta")

    alerta_temp = temp_producto is not None and temp_producto > TEMP_MAX_C
    alerta_puerta = puerta == "abierta"

    if alerta_temp and alerta_puerta:
        return "ALERTA_GENERAL"

    if alerta_temp:
        return "ALERTA_TEMPERATURA"

    if alerta_puerta:
        return "ALERTA_PUERTA_ABIERTA"

    return "NORMAL"


def build_mqtt_payload(mcu_reading):
    """
    Construye el JSON final que publicará el MPU.
    El MCU entrega datos de sensores; el MPU agrega timestamp, device_id y estado.
    """
    estado = determine_state(mcu_reading)

    payload = {
        "schema_version": "1.0",
        "device_id": DEVICE_ID,
        "timestamp": utc_now_iso(),
        "source": {
            "board": "Arduino UNO Q",
            "mcu": "STM32U585",
            "mpu": "Qualcomm QRB2210"
        },
        "mcu": {
            "sample_id": mcu_reading.get("sample_id"),
            "mcu_uptime_ms": mcu_reading.get("mcu_uptime_ms"),
            "last_update_ms": mcu_reading.get("last_update_ms")
        },
        "data": mcu_reading.get("data", {}),
        "valid": mcu_reading.get("valid", {}),
        "errors": mcu_reading.get("errors", []),
        "estado": estado
    }

    return payload


def read_mcu_cache():
    """
    Solicita al MCU la última lectura cacheada.
    Esta función NO lee sensores directamente.
    Solo consume lo que el MCU ya dejó preparado.
    """
    raw_response = Bridge.call(RPC_METHOD)

    if raw_response is None:
        raise RuntimeError("El MCU no devolvió respuesta por Bridge/RPC")

    if isinstance(raw_response, dict):
        return raw_response

    raw_text = str(raw_response)

    try:
        return json.loads(raw_text)
    except json.JSONDecodeError as e:
        raise RuntimeError(f"Respuesta del MCU no es JSON válido: {raw_text}") from e


# =====================================================
# MQTT callbacks
# =====================================================

def on_connect(client, userdata, flags, rc, properties=None):
    global mqtt_connected

    # En Paho 2.x rc puede ser objeto; en versiones anteriores puede ser entero.
    rc_value = getattr(rc, "value", rc)

    if rc_value == 0:
        mqtt_connected = True
        print(f"[MQTT] Conectado a {MQTT_BROKER}:{MQTT_PORT}")
    else:
        mqtt_connected = False
        print(f"[MQTT] Error de conexión. Código: {rc}")


def on_disconnect(client, userdata, rc, properties=None, reason_code=None):
    global mqtt_connected
    mqtt_connected = False
    print("[MQTT] Desconectado del broker")


def create_mqtt_client():
    """
    Crea cliente MQTT compatible con Paho 2.x.
    """
    client_id = f"{DEVICE_ID}-mpu"

    try:
        client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id=client_id,
            protocol=mqtt.MQTTv311
        )
    except Exception:
        client = mqtt.Client(
            client_id=client_id,
            protocol=mqtt.MQTTv311
        )

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect

    if MQTT_USERNAME and MQTT_PASSWORD:
        client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)

    return client


def connect_mqtt():
    global mqtt_client

    mqtt_client = create_mqtt_client()

    print(f"[MQTT] Conectando a {MQTT_BROKER}:{MQTT_PORT}...")
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)

    # Mantiene la red MQTT activa en segundo plano.
    mqtt_client.loop_start()


def publish_payload(payload):
    """
    Publica:
    - Lecturas periódicas en /sensores con QoS 0
    - Estado general en /estado con QoS 1
    - Alertas en /alertas con QoS 1
    """
    if not mqtt_connected:
        print("[MQTT] No conectado. Se omite publicación.")
        return

    payload_text = json.dumps(payload, ensure_ascii=False)

    # Publicación periódica de sensores
    mqtt_client.publish(
        TOPIC_SENSORES,
        payload_text,
        qos=0,
        retain=False
    )

    # Publicación de estado resumido
    estado_payload = {
        "device_id": payload["device_id"],
        "timestamp": payload["timestamp"],
        "estado": payload["estado"],
        "sample_id": payload["mcu"]["sample_id"]
    }

    mqtt_client.publish(
        TOPIC_ESTADO,
        json.dumps(estado_payload, ensure_ascii=False),
        qos=1,
        retain=False
    )

    # Publicación de alerta solo si aplica
    if payload["estado"] != "NORMAL":
        alert_payload = {
            "device_id": payload["device_id"],
            "timestamp": payload["timestamp"],
            "estado": payload["estado"],
            "data": payload["data"],
            "valid": payload["valid"],
            "errors": payload["errors"]
        }

        mqtt_client.publish(
            TOPIC_ALERTAS,
            json.dumps(alert_payload, ensure_ascii=False),
            qos=1,
            retain=False
        )

    print(f"[MQTT] Publicado en {TOPIC_SENSORES}")
    print(payload_text)


# =====================================================
# Loop principal del MPU
# =====================================================

def loop():
    try:
        mcu_reading = read_mcu_cache()
        payload = build_mqtt_payload(mcu_reading)
        publish_payload(payload)

    except Exception as e:
        print(f"[ERROR] {e}")

    time.sleep(PUBLISH_INTERVAL_SECONDS)


# =====================================================
# Inicio de aplicación
# =====================================================

def setup():
    print("=======================================")
    print("MPU - Arduino UNO Q")
    print("Publicador MQTT - Cadena de Frio")
    print("=======================================")
    print(f"DEVICE_ID: {DEVICE_ID}")
    print(f"RPC_METHOD: {RPC_METHOD}")
    print(f"MQTT_BROKER: {MQTT_BROKER}")
    print(f"MQTT_BASE_TOPIC: {BASE_TOPIC}")
    print("=======================================")

    connect_mqtt()


setup()
App.run(user_loop=loop)