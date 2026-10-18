"""
Cliente MQTT en Python (TLS) para el ESP32 simulado en Wokwi.

Author: Antonio Alejandro Saenz Camero
Afiliación: Universidad Nacional de Ingeniería (UNI)
Curso: Internet de las Cosas (CBS37)

"""
import json
import os
import sys
import uuid
from datetime import datetime

import paho.mqtt.client as mqtt

# ---- Configuracion (debe coincidir con el sketch del ESP32) ----
BROKER_HOST = "broker.hivemq.com"
BROKER_PORT = 8883
CA_FILE = "AmazonRootCA1.pem"       # en la misma carpeta que este script
TOPIC_PREFIX = "antonio-saenz/"     # igual que TOPIC_PREFIX del sketch
TOPIC_TEMP = TOPIC_PREFIX + "casa/temperatura"   # ESP32 -> Python
TOPIC_LED = TOPIC_PREFIX + "casa/LED"            # Python -> ESP32 (distingue mayusculas)


def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        print(f"Conexion rechazada: {reason_code}")
        return
    print(f"Conectado a {BROKER_HOST}:{BROKER_PORT} con TLS")
    # Suscribirse aqui garantiza que se repita si la conexion se reinicia
    client.subscribe(TOPIC_TEMP, qos=0)
    print(f"Suscrito a: {TOPIC_TEMP}")
    print(f"Comandos al LED se envian a: {TOPIC_LED}")
    print("Escribe ON, OFF o salir y presiona Enter.\n")


def on_disconnect(client, userdata, disconnect_flags, reason_code, properties):
    if reason_code.is_failure:
        print(f"Conexion perdida ({reason_code}). paho intentara reconectar...")
    else:
        print("Desconectado del broker de forma voluntaria.")


def on_message(client, userdata, msg):
    hora = datetime.now().strftime("%H:%M:%S")
    texto = msg.payload.decode("utf-8", errors="replace")
    try:
        d = json.loads(texto)
        temperatura = d["temperatura"]
        unidad_t = d.get("unidad_temperatura", d.get("unidad", "C"))
        linea = f"[{hora}] {msg.topic} -> Temperatura: {temperatura} {unidad_t}"
        if d.get("humedad") is not None:
            unidad_h = d.get("unidad_humedad", "%")
            linea += f" | Humedad: {d['humedad']} {unidad_h}"
        print(linea)
    except (json.JSONDecodeError, KeyError, TypeError, AttributeError):
        print(f"[{hora}] {msg.topic} -> mensaje con formato no esperado: {texto}")


def enviar_comando(client, comando):
    """Publica ON u OFF en el tema del LED y confirma que el broker lo recibio."""
    info = client.publish(TOPIC_LED, comando, qos=1)
    info.wait_for_publish(timeout=5)
    if info.is_published():
        print(f"Comando enviado -> {TOPIC_LED}: {comando}")
    else:
        print("No se pudo enviar el comando (sin confirmacion del broker).")


def main():
    if not os.path.isfile(CA_FILE):
        print(f"No se encontro el archivo de CA '{CA_FILE}'.")
        print("Guardalo en la misma carpeta que este script.")
        sys.exit(1)

    # client_id aleatorio: en un broker publico, dos clientes con el mismo id se desconectan entre si
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"python-client-{uuid.uuid4().hex[:8]}",
    )
    client.tls_set(ca_certs=CA_FILE)  # valida el certificado del broker con la CA indicada

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    try:
        client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
    except OSError as e:
        print(f"No se pudo conectar: {e}")
        sys.exit(1)

    client.loop_start()  # la red corre en un hilo; el hilo principal lee los comandos
    try:
        while True:
            comando = input().strip().upper()
            if comando in ("ON", "OFF"):
                enviar_comando(client, comando)
            elif comando in ("SALIR", "EXIT", "Q"):
                break
            elif comando:
                print("Comando no valido. Usa ON, OFF o salir.")
    except (KeyboardInterrupt, EOFError):
        print()
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()