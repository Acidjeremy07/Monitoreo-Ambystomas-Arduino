#include <SoftwareSerial.h>
#include <EEPROM.h>
#include "GravityTDS.h"
#include <OneWire.h>
#include <DallasTemperature.h>

/***********Calibracion TDS ***************
     Calibration CMD:
     enter -> ingrese enter para inciar la calbración
     cal:tds value -> Calibrar con el valor de TDS conocido (25°C). Ejemplo: cal:707
     exit -> Guardar los parámetros y salir del modo de calibración
 ****************************************************/
#define DEBUG true

// Pin configurations
#define TdsSensorPin A1
#define PHPin A0
#define ODin A2
#define TempSensorPin 22
#define TurbidityPin A4 // Pin de turbidez

// pH sensor calibration constants
const float b = 42.59;
const float m = -9.53;
float temperature = 25,tdsValue = 0;
int ini = 0;
float calibracion;
int connectionId;
float ntu;

// WiFi module
SoftwareSerial esp8266(19, 18); // RX1, TX1 (pins 18 and 19 respectively on Arduino Mega)

// TDS sensor
GravityTDS gravityTds;

// Temperature sensor
OneWire oneWire(TempSensorPin);
DallasTemperature sensors(&oneWire);

// Initialization flag
int initialized = 0;

void setup() {
  Serial.begin(115200);
  esp8266.begin(115200);
  sensors.begin();
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);
  gravityTds.setAdcRange(1024);
  gravityTds.begin();
  // Reset ESP8266
  sendCommand("AT+RST\r\n", 2000, DEBUG);
  // Set ESP8266 mode
  sendCommand("AT+CWMODE=1\r\n", 1000, DEBUG);
  // Connect to WiFi network
  sendCommand("AT+CWJAP=\"YourSSID\",\"YourPassword\"\r\n", 3000, DEBUG);
  delay(10000);
  // Check IP address
  sendCommand("AT+CIFSR\r\n", 1000, DEBUG);
  // Enable multiple connections
  sendCommand("AT+CIPMUX=1\r\n", 1000, DEBUG);
  // Start server on port 80
  sendCommand("AT+CIPSERVER=1,80\r\n", 1000, DEBUG);
  Serial.println("Server Ready");
}

String prepareHTMLContent(float ph, float tds, float temperature, float oxygen, float turbidity) {
  String htmlContent = "<!DOCTYPE html><html><head><title>Mediciones del Sensor</title></head><body>";
  htmlContent += "<h1>Mediciones del Sensor</h1>";
  htmlContent += "<p>pH: " + String(ph, 2) + "</p>";
  htmlContent += "<p>TDS: " + String(tds, 2) + " ppm</p>";
  htmlContent += "<p>Temperatura: " + String(temperature, 2) + " &deg;C</p>";
  htmlContent += "<p>Oxígeno Disuelto: " + String(oxygen, 2) + "%</p>";
  htmlContent += "<p>Turbidez: " + String(turbidity, 2) + "</p>";
  htmlContent += "</body></html>";

  return htmlContent;
}

void sendSensorDataToClient(int connectionId) {
  // Leer datos de los sensores
  float ph = PHpromedio();
  float tds = tdsValue; // Valor de TDS leído en la función loop()
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);
  float oxygen = oxigenodisuelto(calibracion); // Se utiliza la variable calibracion previamente definida
  float turbidity = ntu;

  // Construir el contenido HTML con los datos de los sensores
  String htmlContent = prepareHTMLContent(ph, tds, temperature, oxygen, turbidity);

  // Enviar la respuesta HTTP al cliente
  sendHTTPResponse(connectionId, htmlContent);
}


void loop() {
  if(esp8266.available()) {
    if(esp8266.find("+IPD,")) {
      delay(1000);
      int connectionId = esp8266.read()-48;
      //SENSOR DE PH
      float ph = PHpromedio();  //función que calcula el ph promedio de 10 muestras
      String content = "PH: ";
      content += ph;
      delay(1000);

      //SENSOR DE TOTAL DE SOLIDOS DISUELTOS
      gravityTds.setTemperature(temperature);     // Establece la temperatura y ejecuta la compensación de temperatura.
      gravityTds.update();                        // Muestrea y calcula.
      tdsValue = gravityTds.getTdsValue();        // Luego, obtén el valor.
      content += ", el TDS: ";
      content += tdsValue;
      Serial.print(tdsValue,0);
      Serial.println("ppm");
      delay(1000);

      //SENSOR DE TEMPERATURA
      sensors.requestTemperatures();   //Se envía el comando para leer la temperatura
      float temp= sensors.getTempCByIndex(0); //Se obtiene la temperatura en ºC
      content += " ppm, la temperatura =";
      content += temp;
      Serial.print("Temperatura= ");
      Serial.print(temp);
      Serial.println(" C");
      delay(100);            

      //SENSOR DE TOTAL DE SOLIDOS SUSPENDIDOS
      float tempCal = 25;  // temperatura solucion de calibracion
      float voltCal = 1.10; // voltaje de solucion de calibracion
      float voltage = 0;
      float varTemp = 0; 
      float tempA25 = 0;
      float K = 0;
      float ntu = 0;
      int sensorValue = 0;
      float suma = 0;
      int rep = 30;
      for(int i=0; i<rep; i++) {
        sensorValue = analogRead(A3);// read the input on analog pin 0:
        voltage = sensorValue * (5.0 / 1024.0); // Convert the analog reading (which goes from 0 – 1023) to a voltage (0 – 5V):
        tempA25 =voltCal-varTemp;
        K = -865.68 * tempA25;
        ntu = (-865.68 * voltage) - K;
        if(ntu < 0) {
            ntu = 0;  
        }
        suma = suma + ntu;
      }
      ntu = suma / rep;

      content += "C, los TDS = ";
      content += ntu;
      Serial.println("CRUDO"); // print out the value you read:
      Serial.println(sensorValue); // print out the value you read:
      Serial.println("VOLTAJE A 5V"); // print out the value you read:
      Serial.println(voltage); // print out the value you read:
      Serial.println("NTU"); // print out the value you read:
      Serial.println(round(ntu)); // print out the value you read:
      delay(1000);

      //SENSOR DE OXIGENO DISUELTO
      if(ini == 0) //10 segundos de calibración 
      {
        calibracion = voltajepromedio();
        ini = ini + 1;
      }
      float porcentaje = oxigenodisuelto(calibracion);
      content += "ntu y el % de oxigeno disuelto = ";
      content += porcentaje; 
      delay(1000);
      // Leer datos de los sensores
      ph = PHpromedio();
      float tds = tdsValue; // Valor de TDS leído en la función loop()
      sensors.requestTemperatures();
      float temperature = sensors.getTempCByIndex(0);
      float oxygen = oxigenodisuelto(calibracion); // Se utiliza la variable calibracion previamente definida
      float turbidity = ntu;
      sendSensorDataToClient(connectionId);

    }  
  }

  // Cerrar conexión
  String closeCommand = "AT+CIPCLOSE="; 
  closeCommand += connectionId;
  closeCommand += "\r\n";
  sendCommand(closeCommand, 1000, DEBUG);
}

float PHpromedio() {
  long muestras;       // Variable para las 10 muestras
  float muestrasprom;  // Variable para calcular el promedio de las muestras

  muestras = 0;
  muestrasprom = 0;

  // 10 muestras, una cada 10ms
  for (int x = 0; x < 10; x++) {
    muestras += analogRead(PHPin);
    delay(10);
  }

  muestrasprom = muestras / 10.0;

  float phVoltaje = muestrasprom * (5.0 / 1024.0);  // se calcula el voltaje
  float pHValor = phVoltaje * m + b;  // Aplicando la ecuación se calcula el PH

  impresion(pHValor, phVoltaje);
  return pHValor;
}

void impresion(float ph, float v) {
  Serial.print("Ph: ");
  Serial.print(ph);
  Serial.print("  Voltaje: ");
  Serial.println(v);
}

void impresion(float v) {
  Serial.print("Voltaje: ");
  Serial.println(v);
  Serial.print("mv");
}

float oxigenodisuelto(float vc) {
  long muestras;       // Variable para las 10 muestras
  float muestrasprom;  // Variable para calcular el promedio de las muestras
  float porcentaje;

  muestras = 0;
  muestrasprom = 0;

  // 10 muestras, una cada 10ms
  for (int x = 0; x < 1000; x++) {
    muestras += analogRead(ODin);
    delay(10);
  }

  muestrasprom = muestras / 1000.0;

  float Voltaje = (muestrasprom * (5.0 / 1024.0) * 1000);
  porcentaje = (Voltaje / vc) * 100;
  Serial.print("Porcentaje de DO: ");
  Serial.print("porcentaje");
}

float voltajepromedio() {
  long muestras;       // Variable para las 10 muestras
  float muestrasprom;  // Variable para calcular el promedio de las muestras

  muestras = 0;
  muestrasprom = 0;

  // 10 muestras, una cada 10ms
  for (int x = 0; x < 1000; x++) {
    muestras += analogRead(ODin);
    delay(10);
  }

  muestrasprom = muestras / 1000.0;

  float Voltaje = (muestrasprom * (5.0 / 1024.0) * 1000);  // se calcula el voltaje

  impresion(Voltaje);
  return Voltaje;
}

String sendData(String data, const int timeout, boolean debug) {
  String response = "";

  int dataSize = data.length();
  char dataChar[dataSize + 1];
  data.toCharArray(dataChar, dataSize + 1);

  esp8266.write(dataChar);

  if (debug) {
    Serial.println("\r\n====== HTTP Response From Arduino ======");
    Serial.write(dataChar, dataSize);
    Serial.println("\r\n========================================");
  }

  long int time = millis();

  while ((time + timeout) > millis()) {
    while (esp8266.available()) {
      // The esp has data so display its output to the serial window
      char c = esp8266.read(); // read the next character.
      response += c;
    }
  }

  if (debug) {
    Serial.print(response);
  }

  return response;
}

void sendHTTPResponse(int connectionId, String content) {
  // build HTTP response
  String httpResponse;
  String httpHeader;
  // HTTP Header
  httpHeader = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n";
  httpHeader += "Content-Length: ";
  httpHeader += content.length();
  httpHeader += "\r\n";
  httpHeader += "Connection: close\r\n\r\n";
  httpResponse = httpHeader + content + " "; // There is a bug in this code: the last character of "content" is not sent, I cheated by adding this extra space
  sendCIPData(connectionId, httpResponse);
}

void sendCIPData(int connectionId, String data) {
  String cipSend = "AT+CIPSEND=";
  cipSend += connectionId;
  cipSend += ",";
  cipSend += data.length();
  cipSend += "\r\n";
  sendCommand(cipSend, 1000, DEBUG);
  sendData(data, 1000, DEBUG);
}

String sendCommand(String command, const int timeout, boolean debug) {
  String response = "";

  esp8266.print(command);

  long int time = millis();

  while ((time + timeout) > millis()) {
    while (esp8266.available()) {
      char c = esp8266.read();
      response += c;
    }
  }

  if (debug) {
    Serial.print(response);
  }

  return response;
}
