#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// =====================================================
// Pines
// =====================================================
#define PIN_DS18B20 2
#define PIN_MC38 4

// =====================================================
// Intervalos
// =====================================================
const unsigned long INTERVALO_LECTURA_MS = 2000;

// =====================================================
// Sensores
// =====================================================
OneWire oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);

Adafruit_BMP280 bmp;
bool bmpDetectado = false;

// =====================================================
// Estructura cacheada de datos del MCU
// =====================================================
struct LecturaMCU {
  unsigned long sample_id;
  unsigned long mcu_uptime_ms;
  unsigned long last_update_ms;

  float temperatura_producto_c;
  float temperatura_ambiente_c;
  float presion_hpa;
  float altitud_m;

  int puerta_raw;
  String puerta;

  bool valid_ds18b20;
  bool valid_bmp280;
  bool valid_mc38;

  String errors;
  String json_cache;
};

// Variable global cacheada
LecturaMCU cache;

// Control de tiempo
unsigned long ultimaLecturaMs = 0;

// =====================================================
// Prototipos
// =====================================================
void inicializarCache();
void inicializarSensores();
void actualizarCacheSensores();
void leerDS18B20();
void leerBMP280();
void leerMC38();
void construirJsonCache();
String getReadingJson();
String getStatusJson();
String valorFloatJson(float valor, int decimales, bool valido);
void imprimirDebug();

void setup() {
  Monitor.begin();
  delay(2000);

  Monitor.println("=======================================");
  Monitor.println("MCU - Arduino UNO Q");
  Monitor.println("Sistema IoT Cadena de Frio");
  Monitor.println("Sensores: DS18B20, BMP280, MC-38");
  Monitor.println("=======================================");

  inicializarCache();
  inicializarSensores();

  // Iniciar Bridge/RPC
  Bridge.begin();

  /*
    Metodo principal para el MPU.
    El MPU llamara este metodo para recibir la ultima lectura cacheada.
  */
  Bridge.provide_safe("coldchain.mcu.v1.get_reading", getReadingJson);

  /*
    Alias corto por si despues quieres probar desde Python
    con un nombre mas simple.
  */
  Bridge.provide_safe("get_reading", getReadingJson);

  /*
    Metodo auxiliar para saber si el MCU esta vivo.
  */
  Bridge.provide_safe("coldchain.mcu.v1.get_status", getStatusJson);
  Bridge.provide_safe("get_status", getStatusJson);

  Monitor.println("Bridge/RPC inicializado.");
  Monitor.println("Metodos disponibles:");
  Monitor.println("- coldchain.mcu.v1.get_reading");
  Monitor.println("- get_reading");
  Monitor.println("- coldchain.mcu.v1.get_status");
  Monitor.println("- get_status");

  // Primera lectura inmediata
  actualizarCacheSensores();
  imprimirDebug();

  Monitor.println("=======================================");
  Monitor.println("MCU listo. Esperando solicitudes del MPU.");
  Monitor.println("=======================================");
}

void loop() {
  unsigned long ahora = millis();

  if (ahora - ultimaLecturaMs >= INTERVALO_LECTURA_MS) {
    ultimaLecturaMs = ahora;

    actualizarCacheSensores();
    imprimirDebug();
  }
}

// =====================================================
// Inicializar estructura cacheada
// =====================================================
void inicializarCache() {
  cache.sample_id = 0;
  cache.mcu_uptime_ms = 0;
  cache.last_update_ms = 0;

  cache.temperatura_producto_c = 0.0;
  cache.temperatura_ambiente_c = 0.0;
  cache.presion_hpa = 0.0;
  cache.altitud_m = 0.0;

  cache.puerta_raw = HIGH;
  cache.puerta = "desconocida";

  cache.valid_ds18b20 = false;
  cache.valid_bmp280 = false;
  cache.valid_mc38 = false;

  cache.errors = "";
  cache.json_cache = "{}";
}

// =====================================================
// Inicializar sensores
// =====================================================
void inicializarSensores() {
  // MC-38
  pinMode(PIN_MC38, INPUT_PULLUP);
  cache.valid_mc38 = true;
  Monitor.println("MC-38 inicializado en D4 con INPUT_PULLUP.");

  // DS18B20
  ds18b20.begin();

  int cantidadDS18B20 = ds18b20.getDeviceCount();

  Monitor.print("DS18B20 encontrados: ");
  Monitor.println(cantidadDS18B20);

  if (cantidadDS18B20 > 0) {
    cache.valid_ds18b20 = true;
    Monitor.println("DS18B20 inicializado en D2.");
  } else {
    cache.valid_ds18b20 = false;
    Monitor.println("ADVERTENCIA: No se detecto DS18B20.");
  }

  // BMP280
  Wire.begin();

  Monitor.println("Bus I2C iniciado.");
  Monitor.println("Buscando BMP280 en 0x76...");

  if (bmp.begin(0x76)) {
    bmpDetectado = true;
    cache.valid_bmp280 = true;
    Monitor.println("BMP280 detectado en 0x76.");
  } else {
    Monitor.println("No se encontro BMP280 en 0x76.");
    Monitor.println("Buscando BMP280 en 0x77...");

    if (bmp.begin(0x77)) {
      bmpDetectado = true;
      cache.valid_bmp280 = true;
      Monitor.println("BMP280 detectado en 0x77.");
    } else {
      bmpDetectado = false;
      cache.valid_bmp280 = false;
      Monitor.println("ADVERTENCIA: No se encontro BMP280.");
    }
  }

  if (bmpDetectado) {
    bmp.setSampling(
      Adafruit_BMP280::MODE_FORCED,
      Adafruit_BMP280::SAMPLING_X2,
      Adafruit_BMP280::SAMPLING_X16,
      Adafruit_BMP280::FILTER_OFF,
      Adafruit_BMP280::STANDBY_MS_500
    );

    Monitor.println("BMP280 configurado en modo FORCED.");
  }
}

// =====================================================
// Actualizar cache de sensores
// =====================================================
void actualizarCacheSensores() {
  cache.errors = "";
  cache.sample_id++;
  cache.mcu_uptime_ms = millis();
  cache.last_update_ms = millis();

  leerDS18B20();
  leerBMP280();
  leerMC38();

  construirJsonCache();
}

// =====================================================
// Leer DS18B20
// =====================================================
void leerDS18B20() {
  ds18b20.requestTemperatures();
  delay(750);

  float temp = ds18b20.getTempCByIndex(0);

  if (temp == DEVICE_DISCONNECTED_C) {
    cache.valid_ds18b20 = false;
    cache.temperatura_producto_c = 0.0;

    if (cache.errors.length() > 0) {
      cache.errors += ",";
    }

    cache.errors += "{\"sensor\":\"ds18b20\",\"code\":\"DS18B20_DISCONNECTED\",\"message\":\"No se pudo obtener lectura valida del DS18B20\"}";
  } else {
    cache.valid_ds18b20 = true;
    cache.temperatura_producto_c = temp;
  }
}

// =====================================================
// Leer BMP280
// =====================================================
void leerBMP280() {
  if (!bmpDetectado) {
    cache.valid_bmp280 = false;

    if (cache.errors.length() > 0) {
      cache.errors += ",";
    }

    cache.errors += "{\"sensor\":\"bmp280\",\"code\":\"BMP280_NOT_FOUND\",\"message\":\"BMP280 no detectado\"}";
    return;
  }

  if (bmp.takeForcedMeasurement()) {
    cache.valid_bmp280 = true;
    cache.temperatura_ambiente_c = bmp.readTemperature();
    cache.presion_hpa = bmp.readPressure() / 100.0F;
    cache.altitud_m = bmp.readAltitude(1013.25);
  } else {
    cache.valid_bmp280 = false;

    if (cache.errors.length() > 0) {
      cache.errors += ",";
    }

    cache.errors += "{\"sensor\":\"bmp280\",\"code\":\"BMP280_MEASUREMENT_ERROR\",\"message\":\"No se pudo tomar medicion forzada del BMP280\"}";
  }
}

// =====================================================
// Leer MC-38
// =====================================================
void leerMC38() {
  cache.puerta_raw = digitalRead(PIN_MC38);
  cache.valid_mc38 = true;

  if (cache.puerta_raw == LOW) {
    cache.puerta = "cerrada";
  } else {
    cache.puerta = "abierta";
  }
}

// =====================================================
// Construir JSON cacheado
// =====================================================
void construirJsonCache() {
  String json = "{";

  json += "\"schema_version\":\"1.0\",";
  json += "\"sample_id\":";
  json += String(cache.sample_id);
  json += ",";
  json += "\"mcu_uptime_ms\":";
  json += String(cache.mcu_uptime_ms);
  json += ",";
  json += "\"last_update_ms\":";
  json += String(cache.last_update_ms);
  json += ",";

  json += "\"data\":{";

  json += "\"temperatura_producto_c\":";
  json += valorFloatJson(cache.temperatura_producto_c, 2, cache.valid_ds18b20);
  json += ",";

  json += "\"temperatura_ambiente_c\":";
  json += valorFloatJson(cache.temperatura_ambiente_c, 2, cache.valid_bmp280);
  json += ",";

  json += "\"presion_hpa\":";
  json += valorFloatJson(cache.presion_hpa, 2, cache.valid_bmp280);
  json += ",";

  json += "\"altitud_m\":";
  json += valorFloatJson(cache.altitud_m, 2, cache.valid_bmp280);
  json += ",";

  json += "\"puerta\":\"";
  json += cache.puerta;
  json += "\",";

  json += "\"puerta_raw\":";
  json += String(cache.puerta_raw);

  json += "},";

  json += "\"valid\":{";
  json += "\"ds18b20\":";
  json += cache.valid_ds18b20 ? "true" : "false";
  json += ",";
  json += "\"bmp280\":";
  json += cache.valid_bmp280 ? "true" : "false";
  json += ",";
  json += "\"mc38\":";
  json += cache.valid_mc38 ? "true" : "false";
  json += "},";

  json += "\"errors\":[";
  json += cache.errors;
  json += "]";

  json += "}";

  cache.json_cache = json;
}

// =====================================================
// Metodo RPC principal
// Devuelve la ultima lectura cacheada.
// IMPORTANTE: no lee sensores aqui.
// =====================================================
String getReadingJson() {
  return cache.json_cache;
}

// =====================================================
// Metodo RPC auxiliar
// =====================================================
String getStatusJson() {
  String json = "{";
  json += "\"service\":\"coldchain.mcu.v1\",";
  json += "\"status\":\"online\",";
  json += "\"sample_id\":";
  json += String(cache.sample_id);
  json += ",";
  json += "\"mcu_uptime_ms\":";
  json += String(millis());
  json += ",";
  json += "\"last_update_ms\":";
  json += String(cache.last_update_ms);
  json += "}";
  return json;
}

// =====================================================
// Convertir float a JSON o null
// =====================================================
String valorFloatJson(float valor, int decimales, bool valido) {
  if (!valido) {
    return "null";
  }

  return String(valor, decimales);
}

// =====================================================
// Debug en Monitor Serial de UNO Q
// =====================================================
void imprimirDebug() {
  Monitor.println("--------- CACHE MCU ---------");

  Monitor.print("sample_id: ");
  Monitor.println(cache.sample_id);

  Monitor.print("DS18B20 temp producto: ");
  if (cache.valid_ds18b20) {
    Monitor.print(cache.temperatura_producto_c);
    Monitor.println(" C");
  } else {
    Monitor.println("ERROR");
  }

  Monitor.print("BMP280 temp ambiente: ");
  if (cache.valid_bmp280) {
    Monitor.print(cache.temperatura_ambiente_c);
    Monitor.println(" C");

    Monitor.print("BMP280 presion: ");
    Monitor.print(cache.presion_hpa);
    Monitor.println(" hPa");

    Monitor.print("BMP280 altitud: ");
    Monitor.print(cache.altitud_m);
    Monitor.println(" m");
  } else {
    Monitor.println("ERROR");
  }

  Monitor.print("MC-38 puerta: ");
  Monitor.print(cache.puerta);
  Monitor.print(" | raw: ");
  Monitor.println(cache.puerta_raw);

  Monitor.print("JSON cacheado: ");
  Monitor.println(cache.json_cache);

  Monitor.println("-----------------------------");
}