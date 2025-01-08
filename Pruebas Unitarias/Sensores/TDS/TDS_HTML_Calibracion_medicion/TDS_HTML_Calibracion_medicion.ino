#include "GravityTDS.h"                   //Libreria para el sensor TDS

#define TdsSensorPin A0                   // Pin para el sensor TDS 
GravityTDS gravityTds;
float temperature = 25, tdsValue = 0;

  // Configuraciones de Gravity TDS
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAdcRange(1024);  // Ajusta según 
  gravityTds.begin();

  // Rutas del servidor
  server.on("/calibrar", HTTP_GET, calibrar);
  server.on("/medir", HTTP_GET, medir);

  server.begin();
  Serial.println("Servidor iniciado");
}

void loop() {
  server.handleClient();
}

void calibrar() {
  // Aquí tu lógica para iniciar la calibración
  Serial.println("Iniciando calibración...");
  server.send(200, "text/plain", "Calibración iniciada");
}

void medir() {
  gravityTds.setTemperature(temperature);
  gravityTds.update();
  tdsValue = gravityTds.getTdsValue();

  Serial.print(tdsValue, 0);
  Serial.println(" ppm");

  server.send(200, "text/plain", String(tdsValue) + " ppm");
}
