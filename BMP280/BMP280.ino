#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

Adafruit_BMP280 bmp;

float tempAnterior = 0;
float presAnterior = 0;

void setup() {
  Monitor.begin();
  delay(2000);

  Monitor.println("Prueba BMP280 - medicion forzada");
  Monitor.println("Usando pines SDA/SCL dedicados del Arduino UNO Q");

  Wire.begin();

  bool encontrado = false;

  Monitor.println("Probando BMP280 en 0x76...");
  if (bmp.begin(0x76)) {
    Monitor.println("BMP280 detectado en 0x76");
    encontrado = true;
  } else {
    Monitor.println("No se encontro en 0x76");
    Monitor.println("Probando BMP280 en 0x77...");

    if (bmp.begin(0x77)) {
      Monitor.println("BMP280 detectado en 0x77");
      encontrado = true;
    }
  }

  if (!encontrado) {
    Monitor.println("No se encontro BMP280.");
    while (1);
  }

  // Modo FORCED: toma una medicion nueva cuando se le pide
  bmp.setSampling(
    Adafruit_BMP280::MODE_FORCED,
    Adafruit_BMP280::SAMPLING_X2,
    Adafruit_BMP280::SAMPLING_X16,
    Adafruit_BMP280::FILTER_OFF,
    Adafruit_BMP280::STANDBY_MS_500
  );

  Monitor.println("Iniciando lecturas...");
}

void loop() {
  // Forzar medicion nueva
  if (bmp.takeForcedMeasurement()) {
    float temperatura = bmp.readTemperature();
    float presion = bmp.readPressure() / 100.0F;
    float altitud = bmp.readAltitude(1013.25);

    Monitor.print("Temperatura: ");
    Monitor.print(temperatura, 4);
    Monitor.println(" C");

    Monitor.print("Presion: ");
    Monitor.print(presion, 4);
    Monitor.println(" hPa");

    Monitor.print("Altitud aproximada: ");
    Monitor.print(altitud, 2);
    Monitor.println(" m");

    Monitor.print("Cambio temperatura: ");
    Monitor.print(temperatura - tempAnterior, 4);
    Monitor.println(" C");

    Monitor.print("Cambio presion: ");
    Monitor.print(presion - presAnterior, 4);
    Monitor.println(" hPa");

    tempAnterior = temperatura;
    presAnterior = presion;

    Monitor.println("-------------------------");
  } else {
    Monitor.println("Error al tomar medicion forzada.");
  }

  delay(1000);
}