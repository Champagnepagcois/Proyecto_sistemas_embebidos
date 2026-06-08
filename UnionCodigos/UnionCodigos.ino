#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// =====================
// Pines
// =====================
#define PIN_DS18B20 2
#define PIN_MC38 4

// =====================
// DS18B20
// =====================
OneWire oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);

// =====================
// BMP280
// =====================
Adafruit_BMP280 bmp;
bool bmpDetectado = false;

// =====================
// Variables
// =====================
float tempProducto = 0.0;
float tempAmbiente = 0.0;
float presion = 0.0;
float altitud = 0.0;
int estadoPuerta = HIGH;

void setup() {
  Monitor.begin();
  delay(2000);

  Monitor.println("=================================");
  Monitor.println("Sistema IoT - Cadena de Frio");
  Monitor.println("Arduino UNO Q");
  Monitor.println("Sensores: DS18B20, BMP280, MC-38");
  Monitor.println("=================================");

  // =====================
  // Inicializar MC-38
  // =====================
  pinMode(PIN_MC38, INPUT_PULLUP);
  Monitor.println("MC-38 inicializado en pin D4");

  // =====================
  // Inicializar DS18B20
  // =====================
  ds18b20.begin();

  int cantidadDS18B20 = ds18b20.getDeviceCount();

  Monitor.print("DS18B20 encontrados: ");
  Monitor.println(cantidadDS18B20);

  if (cantidadDS18B20 == 0) {
    Monitor.println("ADVERTENCIA: No se detecto DS18B20");
  } else {
    Monitor.println("DS18B20 inicializado correctamente en pin D2");
  }

  // =====================
  // Inicializar BMP280
  // =====================
  Wire.begin();

  Monitor.println("Bus I2C iniciado");
  Monitor.println("Buscando BMP280...");

  if (bmp.begin(0x76)) {
    Monitor.println("BMP280 detectado en direccion 0x76");
    bmpDetectado = true;
  } else if (bmp.begin(0x77)) {
    Monitor.println("BMP280 detectado en direccion 0x77");
    bmpDetectado = true;
  } else {
    Monitor.println("ADVERTENCIA: No se encontro BMP280");
    bmpDetectado = false;
  }

  if (bmpDetectado) {
    bmp.setSampling(
      Adafruit_BMP280::MODE_FORCED,
      Adafruit_BMP280::SAMPLING_X2,
      Adafruit_BMP280::SAMPLING_X16,
      Adafruit_BMP280::FILTER_OFF,
      Adafruit_BMP280::STANDBY_MS_500
    );

    Monitor.println("BMP280 configurado en modo FORCED");
  }

  Monitor.println("=================================");
  Monitor.println("Iniciando lecturas...");
  Monitor.println("=================================");
}

void loop() {
  leerDS18B20();
  leerBMP280();
  leerMC38();

  imprimirLecturas();

  delay(2000);
}

// =====================
// Lectura DS18B20
// =====================
void leerDS18B20() {
  ds18b20.requestTemperatures();
  delay(750);

  float lectura = ds18b20.getTempCByIndex(0);

  if (lectura == DEVICE_DISCONNECTED_C) {
    Monitor.println("ERROR: DS18B20 desconectado o no detectado");
    tempProducto = -127.0;
  } else {
    tempProducto = lectura;
  }
}

// =====================
// Lectura BMP280
// =====================
void leerBMP280() {
  if (!bmpDetectado) {
    return;
  }

  if (bmp.takeForcedMeasurement()) {
    tempAmbiente = bmp.readTemperature();
    presion = bmp.readPressure() / 100.0F;
    altitud = bmp.readAltitude(1013.25);
  } else {
    Monitor.println("ERROR: No se pudo tomar medicion del BMP280");
  }
}

// =====================
// Lectura MC-38
// =====================
void leerMC38() {
  estadoPuerta = digitalRead(PIN_MC38);
}

// =====================
// Imprimir datos
// =====================
void imprimirLecturas() {
  Monitor.println("--------- Lecturas ---------");

  Monitor.print("Temperatura producto DS18B20: ");
  if (tempProducto == -127.0) {
    Monitor.println("ERROR");
  } else {
    Monitor.print(tempProducto);
    Monitor.println(" C");
  }

  if (bmpDetectado) {
    Monitor.print("Temperatura ambiente BMP280: ");
    Monitor.print(tempAmbiente);
    Monitor.println(" C");

    Monitor.print("Presion BMP280: ");
    Monitor.print(presion);
    Monitor.println(" hPa");

    Monitor.print("Altitud aproximada: ");
    Monitor.print(altitud);
    Monitor.println(" m");
  } else {
    Monitor.println("BMP280: No detectado");
  }

  Monitor.print("Estado puerta MC-38: ");

  if (estadoPuerta == LOW) {
    Monitor.println("CERRADA");
  } else {
    Monitor.println("ABIERTA");
  }

  Monitor.println("----------------------------");
}