#include <Arduino.h> //

const int dirPin1 = 5;   // Pin de Dirección
const int stepPin1 = 4;  // Pin de Pasos

const int dirPin2 = 7;   // Pin de Dirección
const int stepPin2 = 6;  // Pin de Pasos

// Un motor NEMA 17 típico tiene 200 pasos por vuelta (1.8 grados por paso)
const int pasosPorVuelta = 200; 

void setup() {
  pinMode(stepPin1, OUTPUT);
  pinMode(dirPin1, OUTPUT);
  pinMode(stepPin2, OUTPUT);
  pinMode(dirPin2, OUTPUT);
  pinMode(15, OUTPUT);
}

void loop() {
  //Motor X
  // 1. Girar una vuelta en sentido horario
  digitalWrite(dirPin1, HIGH); // Establece la dirección
  
  for(int x = 0; x < pasosPorVuelta; x++) {
    digitalWrite(stepPin1, HIGH);
    delayMicroseconds(1000);    // Determina la velocidad (menor número = más rápido)
    digitalWrite(stepPin1, LOW);
    delayMicroseconds(1000);
  }
  
  delay(1000); 

  // 2. Girar una vuelta en sentido antihorario
  digitalWrite(dirPin1, LOW); // Cambia la dirección
  
  for(int x = 0; x < pasosPorVuelta; x++) {
    digitalWrite(stepPin1, HIGH);
    delayMicroseconds(1000);
    digitalWrite(stepPin1, LOW);
    delayMicroseconds(1000);
  }
  
  delay(1000); 

  //Motor Y
  // 1. Girar una vuelta en sentido horario
  digitalWrite(dirPin2, HIGH); // Establece la dirección
  
  for(int x = 0; x < pasosPorVuelta; x++) {
    digitalWrite(stepPin2, HIGH);
    delayMicroseconds(1000);    // Determina la velocidad (menor número = más rápido)
    digitalWrite(stepPin2, LOW);
    delayMicroseconds(1000);
  }
  
  delay(1000); 

  // 2. Girar una vuelta en sentido antihorario
  digitalWrite(dirPin2, LOW); // Cambia la dirección
  
  for(int x = 0; x < pasosPorVuelta; x++) {
    digitalWrite(stepPin2, HIGH);
    delayMicroseconds(1000);
    digitalWrite(stepPin2, LOW);
    delayMicroseconds(1000);
  }
  
  delay(1000); 

  //Prueba del solenoide
  digitalWrite(15, HIGH);
  delay(10);
  digitalWrite(15, LOW);
  delay(1000);
}