# Sistema IoT de Cadena de Frío con Arduino UNO Q, MQTT, Mosquitto y SPA

Este proyecto implementa un sistema IoT de monitoreo de cadena de frío usando una **Arduino UNO Q**, sensores físicos, un publicador MQTT en Python, un broker **Eclipse Mosquitto** y una SPA en HTML/JavaScript para visualizar los datos en tiempo real.

La arquitectura final utiliza la parte **MPU/Linux** de la Arduino UNO Q para ejecutar el broker Mosquitto y el script `main.py`, mientras que la parte **MCU/sketch** se encarga de la lectura de sensores y comunicación con Python mediante el bridge correspondiente.

---

## 1. Arquitectura general

```text
Arduino UNO Q
├── MCU / Sketch Arduino
│   ├── Lee sensores físicos
│   │   ├── DS18B20 - temperatura
│   │   ├── BMP280 - temperatura/presión
│   │   └── MC-38 - sensor magnético de puerta
│   └── Envía lecturas al MPU mediante bridge
│
├── MPU / Linux
│   ├── Mosquitto Broker
│   │   ├── Puerto 1883: MQTT normal
│   │   └── Puerto 9001: MQTT sobre WebSockets
│   │
│   └── main.py
│       ├── Recibe datos del MCU
│       ├── Construye mensajes JSON
│       └── Publica en tópicos MQTT
│
└── Red local
    └── Laptop / navegador
        └── SPA index.html
            └── Se conecta por WebSockets a la UNO Q
```

---

## 2. Flujo de comunicación

```text
Sensores físicos
    ↓
MCU de Arduino UNO Q
    ↓
Bridge hacia Python
    ↓
main.py en el MPU/Linux
    ↓
Mosquitto en localhost:1883
    ↓
Clientes MQTT:
    ├── mosquitto_sub
    └── SPA por WebSockets en puerto 9001
```

---

## 3. Componentes utilizados

### Hardware

- Arduino UNO Q.
- ESP32
- Sensor DS18B20 para temperatura.
- Sensor BMP280 para temperatura/presión.
- Sensor magnético MC-38 para detección de apertura/cierre.
- Resistencia de 4.7 kΩ para el DS18B20.
- Protoboard y jumpers.
- 2 Leds Rojos
- 1 Led Blanco
- 1 Led Verde

### Software

- Arduino App Lab.
- Arduino IDE, si se carga o modifica el sketch del MCU.
- Python 3 en el MPU/Linux de la UNO Q.
- Eclipse Mosquitto.
- Mosquitto clients.
- Paho MQTT para Python.
- Paho MQTT JavaScript para la SPA.

---

## 4. Tópicos MQTT usados

Los tópicos siguen una estructura jerárquica:

```text
escom/iot/equipoX/cadena_frio/sensores
escom/iot/equipoX/cadena_frio/estado
escom/iot/equipoX/cadena_frio/alertas
```

Ejemplo de publicación de sensores:

```json
{
  "schema_version": "1.0",
  "device_id": "unoq-cadena-frio-01",
  "timestamp": "2026-06-21T05:00:00Z",
  "temperatura_c": 4.2,
  "presion_hpa": 784.9,
  "puerta": "cerrada",
  "estado": "NORMAL"
}
```

---

## 5. Instalación desde cero en la Arduino UNO Q

> Estos comandos se ejecutan en la terminal Linux/MPU de la Arduino UNO Q.

### 5.1 Actualizar paquetes

```bash
sudo apt update
```

### 5.2 Instalar Mosquitto y clientes MQTT

```bash
sudo apt install -y mosquitto mosquitto-clients
```

### 5.3 Instalar dependencias de Python

```bash
python3 -m pip install --upgrade pip
python3 -m pip install paho-mqtt
```

Si el entorno de App Lab ya trae algunas dependencias instaladas, este paso puede no ser necesario, pero se recomienda validarlo.

---

## 6. Configuración de Mosquitto

El proyecto necesita dos tipos de conexión:

| Puerto | Protocolo | Uso |
|---|---|---|
| 1883 | MQTT normal | Usado por `main.py`, `mosquitto_pub` y `mosquitto_sub` |
| 9001 | MQTT sobre WebSockets | Usado por la SPA en navegador |

### 6.1 Crear configuración

Ejecuta:

```bash
sudo bash -c 'cat > /etc/mosquitto/conf.d/default.conf <<EOF
allow_anonymous true

listener 1883
protocol mqtt

listener 9001
protocol websockets
socket_domain ipv4
EOF'
```

### 6.2 Verificar archivo

```bash
nl -ba /etc/mosquitto/conf.d/default.conf
```

Debe verse así:

```text
     1  allow_anonymous true
     2
     3  listener 1883
     4  protocol mqtt
     5
     6  listener 9001
     7  protocol websockets
     8  socket_domain ipv4
```

### 6.3 Reiniciar Mosquitto

```bash
sudo systemctl reset-failed mosquitto
sudo systemctl restart mosquitto
sudo systemctl status mosquitto --no-pager -l
```

Debe aparecer:

```text
active (running)
```

### 6.4 Verificar puertos

```bash
ss -lntp | grep -E '1883|9001'
```

Resultado esperado:

```text
LISTEN 0 100  0.0.0.0:1883  0.0.0.0:*
LISTEN 0 4096 0.0.0.0:9001  0.0.0.0:*
```

---

## 7. Prueba local del broker MQTT

En una terminal de la UNO Q:

```bash
mosquitto_sub -h localhost -t "test/#" -v
```

En otra terminal:

```bash
mosquitto_pub -h localhost -t "test/hola" -m "hola desde la UNO Q"
```

Si todo funciona, en la terminal del subscriber debe aparecer:

```text
test/hola hola desde la UNO Q
```

---

## 8. Configuración de `main.py`

En el archivo `main.py`, el broker debe apuntar a `localhost`, porque Mosquitto corre en la misma UNO Q:

```python
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
```

Si se usa variable de entorno:

```python
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
```

### 8.1 Ejemplo de conexión MQTT en Python

```python
import paho.mqtt.client as mqtt

MQTT_BROKER = "localhost"
MQTT_PORT = 1883

client = mqtt.Client()
client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
client.loop_start()

client.publish(
    "escom/iot/equipoX/cadena_frio/sensores",
    '{"temperatura_c": 4.2, "estado": "NORMAL"}'
)
```

---

## 9. Ejecución del proyecto

### 9.1 Verificar que Mosquitto está activo

```bash
sudo systemctl status mosquitto --no-pager -l
```

Debe mostrar:

```text
active (running)
```

### 9.2 Ejecutar el publicador Python

Desde Arduino App Lab o desde terminal, ejecutar el script principal:

```bash
python3 main.py
```

Salida esperada:

```text
MPU - Arduino UNO Q
Publicador MQTT - Cadena de Frio
[MQTT] Conectando a localhost:1883...
[MQTT] Conectado a localhost:1883
```

### 9.3 Ver mensajes desde terminal

En otra terminal de la UNO Q:

```bash
mosquitto_sub -h localhost -t "escom/iot/equipoX/cadena_frio/#" -v
```

Deberías ver mensajes parecidos a:

```text
escom/iot/equipoX/cadena_frio/sensores {"temperatura_c":4.2,"puerta":"cerrada","estado":"NORMAL"}
escom/iot/equipoX/cadena_frio/estado {"device_id":"unoq-cadena-frio-01","estado":"NORMAL"}
```

---

## 10. Configuración de la SPA

La SPA se conecta al broker usando WebSockets, no MQTT TCP normal.

Por eso debe usar:

```text
IP_DE_LA_UNO_Q:9001
```

No debe usar `localhost` si el archivo HTML se abre desde una laptop, porque `localhost` en el navegador significa la laptop, no la Arduino UNO Q.

### 10.1 Obtener IP de la UNO Q

En la terminal de la UNO Q:

```bash
hostname -I
```

Ejemplo:

```text
192.168.1.80
```

### 10.2 Configuración en `index.html`

En el archivo de la SPA:

```javascript
const MQTT_BROKER = "192.168.1.80"; // IP de la Arduino UNO Q
const MQTT_PORT = 9001;
const MQTT_PATH = "/mqtt";
```

La conexión real será:

```text
ws://192.168.1.80:9001/mqtt
```

### 10.3 Ejemplo base con Paho JavaScript

```html
<script src="https://unpkg.com/paho-mqtt/mqttws31.min.js"></script>

<script>
  const MQTT_BROKER = "192.168.1.80";
  const MQTT_PORT = 9001;
  const MQTT_PATH = "/mqtt";

  const clientId = "spa-cadena-frio-" + Math.random().toString(16).slice(2);

  const client = new Paho.MQTT.Client(
    MQTT_BROKER,
    MQTT_PORT,
    MQTT_PATH,
    clientId
  );

  client.onConnectionLost = function (responseObject) {
    console.error("Conexion perdida:", responseObject.errorMessage);
  };

  client.onMessageArrived = function (message) {
    console.log("Topic:", message.destinationName);
    console.log("Payload:", message.payloadString);
  };

  client.connect({
    onSuccess: function () {
      console.log("Conectado a Mosquitto por WebSockets");
      client.subscribe("escom/iot/equipoX/cadena_frio/#");
    },
    onFailure: function (error) {
      console.error("Error de conexion MQTT WebSocket:", error.errorMessage);
    },
    reconnect: true
  });
</script>
```

---

## 11. Ejecución de la SPA

1. Verifica que Mosquitto esté activo en la UNO Q.
2. Verifica que el puerto 9001 esté escuchando:

```bash
ss -lntp | grep 9001
```

3. Abre `index.html` en tu navegador.
4. Revisa la consola del navegador.
5. Debe aparecer:

```text
Conectado a Mosquitto por WebSockets
```

6. Cuando `main.py` publique lecturas, la SPA debe recibir los mensajes.

---

## 12. Pruebas recomendadas

### 12.1 Prueba MQTT normal

```bash
mosquitto_sub -h localhost -t "test/#" -v
mosquitto_pub -h localhost -t "test/hola" -m "prueba MQTT"
```

### 12.2 Prueba de tópicos del proyecto

```bash
mosquitto_sub -h localhost -t "escom/iot/equipoX/cadena_frio/#" -v
```

### 12.3 Prueba de WebSockets

Verifica que el puerto 9001 esté activo:

```bash
ss -lntp | grep 9001
```

Después abre la SPA en el navegador usando la IP de la UNO Q.

---

## 13. Errores comunes y solución

### Error: `ConnectionRefusedError: [Errno 111] Connection refused`

Significa que el script Python intenta conectarse a un broker que no está escuchando en esa IP/puerto.

Solución:

```bash
sudo systemctl status mosquitto --no-pager -l
ss -lntp | grep 1883
```

Si Mosquitto corre en la misma UNO Q, en Python usa:

```python
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
```

---

### Error: la SPA no conecta

Causas comunes:

- Se usó `localhost` en el navegador.
- El puerto 9001 no está activo.
- El broker no tiene `protocol websockets`.
- La laptop no está en la misma red que la UNO Q.

Solución:

```bash
hostname -I
ss -lntp | grep 9001
```

En la SPA usar:

```javascript
const MQTT_BROKER = "IP_DE_LA_UNO_Q";
const MQTT_PORT = 9001;
```

---

### Error: Mosquitto no arranca después de editar configuración

Ver el error real:

```bash
sudo systemctl status mosquitto --no-pager -l
sudo journalctl -xeu mosquitto.service --no-pager -n 80
```

Probar Mosquitto manualmente:

```bash
sudo mosquitto -c /etc/mosquitto/mosquitto.conf -v
```

---

### Error: dos archivos `.conf` abren el mismo puerto

Revisar archivos:

```bash
ls -la /etc/mosquitto/conf.d/
cat /etc/mosquitto/conf.d/default.conf
```

Si hay dos archivos con `listener 1883`, dejar solo uno activo.

---

## 14. Configuración final esperada de Mosquitto

Archivo:

```text
/etc/mosquitto/conf.d/default.conf
```

Contenido:

```conf
allow_anonymous true

listener 1883
protocol mqtt

listener 9001
protocol websockets
socket_domain ipv4
```

---

## 15. Comandos rápidos para levantar todo

```bash
sudo systemctl restart mosquitto
sudo systemctl status mosquitto --no-pager -l
ss -lntp | grep -E '1883|9001'
python3 main.py
```

Para escuchar mensajes:

```bash
mosquitto_sub -h localhost -t "escom/iot/equipoX/cadena_frio/#" -v
```


---

## 16. Seguridad

La configuración:

```conf
allow_anonymous true
```

se usa únicamente para pruebas locales y demostración académica.

Para un entorno real se recomienda:

- Desactivar acceso anónimo.
- Crear usuarios y contraseñas.
- Usar TLS.
- Restringir puertos en firewall.
- Separar tópicos por dispositivo.
- Usar ACLs para controlar permisos de publicación/suscripción.

---

## 17. Video demostracion

- [Sistema en funcionamiento](https://drive.google.com/file/d/1dIe3g6QmaefivfQdvZpgBstr7TCvlOjW/view?usp=drive_link)
- [Demostracion datos MQTT](https://drive.google.com/file/d/1E8U9826E8PnGQmyKXVg0sOsadkRmpNNI/view?usp=drive_link)
- [Funcionamiento con Dashboard](https://drive.google.com/file/d/1KYzGayhtFmO6vL5F6Fclj1Vr62sjfByy/view?usp=drive_link)
