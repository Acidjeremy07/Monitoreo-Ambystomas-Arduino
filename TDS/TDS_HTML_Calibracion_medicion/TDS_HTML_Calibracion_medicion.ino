#include <ESP8266WiFi.h>                  //Libreria para conectarse a internet
#include <ESP8266WebServer.h>             //Libreria para el ESP01 funcione como server
#include "GravityTDS.h"                   //Libreria para el sensor TDS

#define TdsSensorPin A0                   // Pin para el sensor TDS 
GravityTDS gravityTds;
float temperature = 25, tdsValue = 0;

const char* ssid = "TU_SSID";             // SSID de la red a utilizar 
const char* password = "TU_PASSWORD";     // Contraseña de la red a utilizar 

ESP8266WebServer server(80);              // Crea un servidor web en el puerto 80

void setup() {                            //Función para configuración
  Serial.begin(115200);                   //Velocidad del serial
  WiFi.begin(ssid, password);             //Le da el nombre y clave de la red
  while (WiFi.status() != WL_CONNECTED) { //
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi conectado");       //Indica cuando ya se esta conectado a internet

  // Configuraciones de Gravity TDS
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(3.3);  // Ajusta a 3.3V para ESP8266
  gravityTds.setAdcRange(1024);  // Ajusta según tu ADC
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
