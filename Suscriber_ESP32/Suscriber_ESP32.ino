#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <math.h>

// =====================================================
// WiFi
// =====================================================
const char* WIFI_SSID = "INFINITUM6388";
const char* WIFI_PASSWORD = "YRTQy3dXNX";

// =====================================================
// MQTT
// =====================================================
const char* MQTT_BROKER = "192.168.1.107";
const int MQTT_PORT = 1883;

const char* CLIENT_ID = "esp32-leds-cadena-frio";

// IMPORTANTE:
// Solo escuchamos /sensores porque trae TODO el estado completo.
const char* TOPIC_SENSORES = "escom/iot/equipoX/cadena_frio/sensores";

// =====================================================
// Pines de LEDs
// =====================================================
const int LED_ROJO_1 = 5;   // Producto fuera de rango
const int LED_ROJO_2 = 18;  // Ambiente fuera de rango
const int LED_BLANCO = 19;  // Puerta abierta
const int LED_VERDE  = 21;  // Todo OK

// Si tus LEDs están conectados así:
// GPIO -> resistencia -> LED -> GND
const int LED_ON = HIGH;
const int LED_OFF = LOW;

// Si algún LED prende al revés, cambia a:
// const int LED_ON = LOW;
// const int LED_OFF = HIGH;

// =====================================================
// Cliente WiFi/MQTT
// =====================================================
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// =====================================================
// Apagar todos los LEDs
// =====================================================
void apagarLeds() {
  digitalWrite(LED_ROJO_1, LED_OFF);
  digitalWrite(LED_ROJO_2, LED_OFF);
  digitalWrite(LED_BLANCO, LED_OFF);
  digitalWrite(LED_VERDE, LED_OFF);
}

// =====================================================
// Prueba física de LEDs
// =====================================================
void probarLeds() {
  Serial.println("Probando LEDs...");

  apagarLeds();

  digitalWrite(LED_ROJO_1, LED_ON);
  delay(500);
  digitalWrite(LED_ROJO_1, LED_OFF);

  digitalWrite(LED_ROJO_2, LED_ON);
  delay(500);
  digitalWrite(LED_ROJO_2, LED_OFF);

  digitalWrite(LED_BLANCO, LED_ON);
  delay(500);
  digitalWrite(LED_BLANCO, LED_OFF);

  digitalWrite(LED_VERDE, LED_ON);
  delay(500);
  digitalWrite(LED_VERDE, LED_OFF);

  Serial.println("Prueba de LEDs terminada.");
}

// =====================================================
// Leer booleano de forma segura
// =====================================================
bool leerBool(JsonVariantConst valor, bool defaultValue = false) {
  if (valor.isNull()) {
    return defaultValue;
  }

  if (valor.is<bool>()) {
    return valor.as<bool>();
  }

  if (valor.is<int>()) {
    return valor.as<int>() != 0;
  }

  if (valor.is<const char*>()) {
    String texto = valor.as<String>();
    texto.toLowerCase();

    if (texto == "true" || texto == "1" || texto == "si" || texto == "abierta" || texto == "open") {
      return true;
    }

    if (texto == "false" || texto == "0" || texto == "no" || texto == "cerrada" || texto == "closed") {
      return false;
    }
  }

  return defaultValue;
}

// =====================================================
// Aplicar estado a LEDs
// =====================================================
void aplicarLeds(
  String estado,
  bool puertaAbierta,
  bool productoFueraRango,
  bool ambienteFueraRango,
  bool hayMasDeDosErrores,
  bool sensoresValidos
) {
  // =====================================================
  // Cada LED es independiente
  // =====================================================

  // Rojo 1: temperatura de PRODUCTO fuera de rango
  bool ledRojo1 = productoFueraRango;

  // Rojo 2: temperatura de AMBIENTE fuera de rango
  bool ledRojo2 = ambienteFueraRango;

  // Blanco: puerta abierta (sin importar la temperatura)
  bool ledBlanco = puertaAbierta;

  // Verde: SOLO cuando ambas temperaturas están en rango
  // (se agrega sensoresValidos para no prender verde si un sensor murió)
  bool ledVerde = (!productoFueraRango && !ambienteFueraRango && sensoresValidos);

  // =====================================================
  // Aplicar salidas físicas
  // =====================================================
  digitalWrite(LED_ROJO_1, ledRojo1 ? LED_ON : LED_OFF);
  digitalWrite(LED_ROJO_2, ledRojo2 ? LED_ON : LED_OFF);
  digitalWrite(LED_BLANCO, ledBlanco ? LED_ON : LED_OFF);
  digitalWrite(LED_VERDE, ledVerde ? LED_ON : LED_OFF);

  // =====================================================
  // Debug
  // =====================================================
  Serial.println("========== RESULTADO LEDS ==========");
  Serial.print("Estado: ");                  Serial.println(estado);
  Serial.print("Puerta abierta: ");          Serial.println(puertaAbierta ? "SI" : "NO");
  Serial.print("Producto fuera de rango: "); Serial.println(productoFueraRango ? "SI" : "NO");
  Serial.print("Ambiente fuera de rango: "); Serial.println(ambienteFueraRango ? "SI" : "NO");
  Serial.print("Sensores válidos: ");        Serial.println(sensoresValidos ? "SI" : "NO");
  Serial.print("LED Rojo 1 (producto): ");   Serial.println(ledRojo1 ? "ON" : "OFF");
  Serial.print("LED Rojo 2 (ambiente): ");   Serial.println(ledRojo2 ? "ON" : "OFF");
  Serial.print("LED Blanco (puerta): ");     Serial.println(ledBlanco ? "ON" : "OFF");
  Serial.print("LED Verde (OK temps): ");    Serial.println(ledVerde ? "ON" : "OFF");
  Serial.println("====================================");
}

// =====================================================
// Procesar JSON de /sensores
// =====================================================
void procesarJson(JsonDocument& doc) {
  String estado = doc["estado"] | "DESCONOCIDO";

  JsonObjectConst data = doc["data"].as<JsonObjectConst>();
  JsonObjectConst rangos = doc["rangos"].as<JsonObjectConst>();
  JsonObjectConst alertas = doc["alertas"].as<JsonObjectConst>();
  JsonObjectConst valid = doc["valid"].as<JsonObjectConst>();

  // =====================================================
  // Validación de sensores
  // =====================================================
  bool ds18b20Ok = leerBool(valid["ds18b20"], false);
  bool bmp280Ok = leerBool(valid["bmp280"], false);
  bool mc38Ok = leerBool(valid["mc38"], false);

  bool sensoresValidos = ds18b20Ok && bmp280Ok && mc38Ok;

  // =====================================================
  // Puerta
  // Se revisa por alertas y también por data.
  // En tu MCU: puerta_raw HIGH/1 = abierta, LOW/0 = cerrada.
  // =====================================================
  bool puertaPorAlerta = leerBool(alertas["puerta_abierta"], false);

  String puertaTexto = data["puerta"] | "";
  puertaTexto.toLowerCase();

  int puertaRaw = data["puerta_raw"] | -1;

  bool puertaPorTexto = puertaTexto == "abierta";
  bool puertaPorRaw = puertaRaw == 1;

  bool puertaAbierta = puertaPorAlerta || puertaPorTexto || puertaPorRaw;

  // =====================================================
  // Temperaturas y rangos
  // =====================================================
  float tempProducto = data["temperatura_producto_c"] | NAN;
  float tempAmbiente = data["temperatura_ambiente_c"] | NAN;

  float productoMin = rangos["producto"]["min_c"] | NAN;
  float productoMax = rangos["producto"]["max_c"] | NAN;

  float ambienteMin = rangos["ambiente"]["min_c"] | NAN;
  float ambienteMax = rangos["ambiente"]["max_c"] | NAN;

  // Primero leemos las alertas que ya vienen calculadas
  bool productoFueraPorAlerta = leerBool(alertas["temperatura_producto_fuera_rango"], false);
  bool ambienteFueraPorAlerta = leerBool(alertas["temperatura_ambiente_fuera_rango"], false);

  // También calculamos en el ESP32 como respaldo
  bool productoFueraPorCalculo = false;
  bool ambienteFueraPorCalculo = false;

  if (!isnan(tempProducto) && !isnan(productoMin) && !isnan(productoMax)) {
    productoFueraPorCalculo = tempProducto < productoMin || tempProducto > productoMax;
  }

  if (!isnan(tempAmbiente) && !isnan(ambienteMin) && !isnan(ambienteMax)) {
    ambienteFueraPorCalculo = tempAmbiente < ambienteMin || tempAmbiente > ambienteMax;
  }

  bool productoFueraRango = productoFueraPorAlerta || productoFueraPorCalculo;
  bool ambienteFueraRango = ambienteFueraPorAlerta || ambienteFueraPorCalculo;

  // =====================================================
  // Errores
  // =====================================================
  int totalErrores = 0;

  if (doc["errors"].is<JsonArray>()) {
    totalErrores = doc["errors"].as<JsonArray>().size();
  }

  bool hayMasDeDosErrores = totalErrores > 2;

  // =====================================================
  // Debug
  // =====================================================
  Serial.println("========== JSON INTERPRETADO ==========");
  Serial.print("estado: ");
  Serial.println(estado);

  Serial.print("puertaTexto: ");
  Serial.println(puertaTexto);

  Serial.print("puertaRaw: ");
  Serial.println(puertaRaw);

  Serial.print("puertaAbierta final: ");
  Serial.println(puertaAbierta ? "true" : "false");

  Serial.print("tempProducto: ");
  Serial.println(tempProducto);

  Serial.print("productoMin: ");
  Serial.println(productoMin);

  Serial.print("productoMax: ");
  Serial.println(productoMax);

  Serial.print("productoFueraPorAlerta: ");
  Serial.println(productoFueraPorAlerta ? "true" : "false");

  Serial.print("productoFueraPorCalculo: ");
  Serial.println(productoFueraPorCalculo ? "true" : "false");

  Serial.print("tempAmbiente: ");
  Serial.println(tempAmbiente);

  Serial.print("ambienteMin: ");
  Serial.println(ambienteMin);

  Serial.print("ambienteMax: ");
  Serial.println(ambienteMax);

  Serial.print("ambienteFueraPorAlerta: ");
  Serial.println(ambienteFueraPorAlerta ? "true" : "false");

  Serial.print("ambienteFueraPorCalculo: ");
  Serial.println(ambienteFueraPorCalculo ? "true" : "false");

  Serial.print("totalErrores: ");
  Serial.println(totalErrores);

  Serial.print("sensoresValidos: ");
  Serial.println(sensoresValidos ? "true" : "false");

  Serial.println("=======================================");

  aplicarLeds(
    estado,
    puertaAbierta,
    productoFueraRango,
    ambienteFueraRango,
    hayMasDeDosErrores,
    sensoresValidos
  );
}

// =====================================================
// Callback MQTT
// =====================================================
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Mensaje recibido en topic: ");
  Serial.println(topic);

  // Seguridad extra:
  // Aunque solo estamos suscritos a /sensores, ignoramos cualquier otro topic.
  if (String(topic) != TOPIC_SENSORES) {
    Serial.println("Topic ignorado. Solo se procesa /sensores.");
    return;
  }

  Serial.print("Tamaño payload: ");
  Serial.print(length);
  Serial.println(" bytes");

  DynamicJsonDocument doc(12288);

  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("Error al parsear JSON: ");
    Serial.println(error.c_str());
    return;
  }

  procesarJson(doc);
}

// =====================================================
// Conectar WiFi
// =====================================================
void conectarWiFi() {
  Serial.print("Conectando a WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int intentos = 0;

  while (WiFi.status() != WL_CONNECTED && intentos < 40) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi conectado");
    Serial.print("IP del ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("ERROR: No se pudo conectar al WiFi");
    Serial.print("WiFi status: ");
    Serial.println(WiFi.status());
  }
}

// =====================================================
// Conectar MQTT
// =====================================================
void conectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.println();
    Serial.println("Intentando conectar a MQTT...");
    Serial.print("Broker: ");
    Serial.println(MQTT_BROKER);
    Serial.print("Puerto: ");
    Serial.println(MQTT_PORT);
    Serial.print("IP ESP32: ");
    Serial.println(WiFi.localIP());

    if (mqttClient.connect(CLIENT_ID)) {
      Serial.println("MQTT conectado");

      bool subSensores = mqttClient.subscribe(TOPIC_SENSORES);

      Serial.print("Suscrito a sensores: ");
      Serial.println(subSensores ? "SI" : "NO");

    } else {
      int estado = mqttClient.state();

      Serial.print("MQTT falló, rc=");
      Serial.println(estado);

      delay(3000);
    }
  }
}

// =====================================================
// Setup
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_ROJO_1, OUTPUT);
  pinMode(LED_ROJO_2, OUTPUT);
  pinMode(LED_BLANCO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);

  apagarLeds();
  probarLeds();

  conectarWiFi();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(callback);

  bool bufferOk = mqttClient.setBufferSize(12288);

  Serial.print("Buffer MQTT 12288: ");
  Serial.println(bufferOk ? "OK" : "ERROR");

  mqttClient.setKeepAlive(60);
}

// =====================================================
// Loop
// =====================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    apagarLeds();
    delay(1000);
    return;
  }

  if (!mqttClient.connected()) {
    conectarMQTT();
  }

  mqttClient.loop();
}