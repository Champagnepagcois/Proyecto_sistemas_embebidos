#include <Arduino_RouterBridge.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Monitor.begin();
  delay(2000);

  Monitor.println("Iniciando prueba DS18B20 en Arduino UNO Q...");
  Monitor.println("Pin DATA configurado en D2");

  sensors.begin();

  int deviceCount = sensors.getDeviceCount();

  Monitor.print("Dispositivos encontrados en bus OneWire: ");
  Monitor.println(deviceCount);

  if (deviceCount == 0) {
    Monitor.println("No se detecto ningun DS18B20.");
    Monitor.println("Revisa cableado, resistencia 4.7k, VCC, GND y pin DATA.");
  }
}

void loop() {
  sensors.requestTemperatures();
  delay(750);

  float tempC = sensors.getTempCByIndex(0);

  if (tempC == DEVICE_DISCONNECTED_C) {
    Monitor.println("ERROR: Sensor desconectado o no detectado.");
  } else {
    Monitor.print("Temperatura: ");
    Monitor.print(tempC);
    Monitor.println(" C");
  }

  Monitor.println("-----------------------");
  delay(2000);
}