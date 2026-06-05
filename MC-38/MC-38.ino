#include <Arduino_RouterBridge.h>

#define Serial Monitor
#define PIN_MC38 4

void setup() {
  Serial.begin(9600);
  delay(2000);

  pinMode(PIN_MC38, INPUT_PULLUP);
}

void loop() {
  int estado = digitalRead(PIN_MC38);

  Serial.print("Lectura digital: ");
  Serial.print(estado);

  if (estado == LOW) {
    Serial.println(" -> CERRADO - La puerta fue cerrada");
  } else {
    Serial.println(" -> ABIERTO - La puerta fue abierta");
  }

  delay(500);
}