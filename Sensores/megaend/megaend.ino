#include <OneWire.h>
#include <DallasTemperature.h>
#include <GravityTDS.h>
#include <EEPROM.h>

// Pines de conexión
#define TdsSensorPin A1      // Total de Sólidos Disueltos (TDS)
#define PHPin A0             // Sensor de pH
#define TempSensorPin 22     // Sensor de Temperatura (OneWire)
#define TurbidityPin A3      // Sensor de Turbidez (TSS)

// Comunicación con el ESP8266-01
#define espSerial Serial1    // Usa Serial1 (pines 18 y 19) para ESP8266-01

// Comunicación con el sensor de OD
#define ODSensorSerial Serial3 // Usa Serial3 (pines 14 y 15) para OD

// Número de serie del dispositivo
const char* deviceSerialNumber = "DEV-AXOLOTL-01";

// Números de serie de los sensores
const char* sensorSerialNumbers[] = {
  "SEN-PH-001",
  "SEN-TEMP-002",
  "SEN-TDS-003",
  "SEN-DO-004",
  "SEN-TURB-005"
};

// Nombres de los parámetros
const char* parameterNames[] = {
  "pH",
  "Temperature",
  "TDS",
  "DissolvedOxygen",
  "Turbidity"
};

// Rangos aceptables
const float minPH = 6.5;
const float maxPH = 8.0;
const float minTemp = 15.0;
const float maxTemp = 18.0;
const float minDO = 80.0; // Porcentaje mínimo de OD
const float maxTDS = 20.0; // ppm máximo
const float maxTurbidity = 10.0; // NTU máximo

// Variables globales para sensores
GravityTDS gravityTds;
OneWire oneWire(TempSensorPin);
DallasTemperature sensors(&oneWire);

float temperature = 25.0;
float tdsValue = 0.0;
float b = 42.59; // Constante de calibración para pH
float m = -9.53; // Pendiente para pH

// Variables para promediar los datos
const unsigned long sendInterval = 1800000; // 30 minutos en milisegundos
unsigned long lastSendTime = 0;
int readingCount = 0;

// Sumas para promediar
float sumPH = 0;
float sumTemperature = 0;
float sumTDS = 0;
float sumDO = 0;
float sumTurbidity = 0;

// Flags de alerta
bool alertPH = false;
bool alertTemp = false;
bool alertTDS = false;
bool alertDO = false;
bool alertTurbidity = false;

// Funciones Prototipo
float readTemperatureSensor();
float readPHSensor();
float readTurbiditySensor();
float readTDSSensor(float temperature);
float readDissolvedOxygen();
String buildJSON(bool isAlert);
void sendJSON(String jsonData);
void handleAlert();

void setup() {
  // Iniciar comunicaciones seriales
  Serial.begin(115200);      // Monitor serie
  espSerial.begin(9600);     // ESP8266-01
  ODSensorSerial.begin(9600); // Sensor de OD

  // Iniciar sensores
  sensors.begin();

  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);
  gravityTds.setAdcRange(1024);
  gravityTds.begin();

  // Esperar a que los sensores se inicien
  delay(2000);
}

void loop() {
  unsigned long currentMillis = millis();

  // Leer sensores cada minuto
  if (currentMillis - lastSendTime >= 60000) { // 1 minuto
    lastSendTime = currentMillis;

    // Leer sensores
    temperature = readTemperatureSensor();
    float pH = readPHSensor();
    tdsValue = readTDSSensor(temperature);
    float doValue = readDissolvedOxygen();
    float turbidity = readTurbiditySensor();

    // Acumular sumas para promediar
    sumTemperature += temperature;
    sumPH += pH;
    sumTDS += tdsValue;
    sumDO += doValue;
    sumTurbidity += turbidity;

    readingCount++;

    // Comprobar si algún valor está fuera de rango
    bool outOfRange = false;

    // Resetear flags de alerta
    alertPH = false;
    alertTemp = false;
    alertTDS = false;
    alertDO = false;
    alertTurbidity = false;

    if (pH < minPH || pH > maxPH) {
      outOfRange = true;
      alertPH = true;
      Serial.println("Alerta: pH fuera de rango");
    }
    if (temperature < minTemp || temperature > maxTemp) {
      outOfRange = true;
      alertTemp = true;
      Serial.println("Alerta: Temperatura fuera de rango");
    }
    if (doValue < minDO) {
      outOfRange = true;
      alertDO = true;
      Serial.println("Alerta: Oxígeno Disuelto fuera de rango");
    }
    if (turbidity > maxTurbidity) {
      outOfRange = true;
      alertTurbidity = true;
      Serial.println("Alerta: Turbidez fuera de rango");
    }
    if (tdsValue > maxTDS) {
      outOfRange = true;
      alertTDS = true;
      Serial.println("Alerta: TDS fuera de rango");
    }

    if (outOfRange) {
      // Enviar datos inmediatamente con alertas
      String jsonData = buildJSON(true);
      sendJSON(jsonData);
      
      // Reiniciar sumas y contador
      sumTemperature = 0;
      sumPH = 0;
      sumTDS = 0;
      sumDO = 0;
      sumTurbidity = 0;
      readingCount = 0;
    } else if (readingCount >= 30) { // 30 minutos
      // Calcular promedios
      float avgTemperature = sumTemperature / readingCount;
      float avgPH = sumPH / readingCount;
      float avgTDS = sumTDS / readingCount;
      float avgDO = sumDO / readingCount;
      float avgTurbidity = sumTurbidity / readingCount;

      // Enviar datos promedio
      String jsonData = buildJSON(false);
      sendJSON(jsonData);

      // Reiniciar sumas y contador
      sumTemperature = 0;
      sumPH = 0;
      sumTDS = 0;
      sumDO = 0;
      sumTurbidity = 0;
      readingCount = 0;
    }
  }
}

// Función para leer el sensor de temperatura
float readTemperatureSensor() {
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  Serial.print("Temperatura: ");
  Serial.println(temp);
  return temp;
}

// Función para leer el sensor de pH
float readPHSensor() {
  long samples = 0;
  for (int x = 0; x < 10; x++) {
    samples += analogRead(PHPin);
    delay(10);
  }
  float phVoltage = (samples / 10.0) * (5.0 / 1024.0);
  float pHValue = phVoltage * m + b;
  Serial.print("pH: ");
  Serial.println(pHValue);
  return pHValue;
}

// Función para leer el sensor de Turbidez (TSS)
float readTurbiditySensor() {
  float tempCal = 25.0;    // Temperatura de solución de calibración
  float voltCal = 1.10;    // Voltaje de solución de calibración
  float varTemp = 0.0; 
  float tempA25 = 0.0;
  float K = 0.0;
  float ntu = 0.0;
  int sensorValue = 0;

  float sum = 0.0;
  int rep = 30;
  for(int i = 0; i < rep; i++) {
    sensorValue = analogRead(TurbidityPin);
    float voltage = sensorValue * (5.0 / 1024.0);
    tempA25 = voltCal - varTemp;
    K = -865.68 * tempA25;
    ntu = (-865.68 * voltage) - K;
    if(ntu < 0) {
      ntu = 0;  
    }
    sum += ntu;
    delay(10);
  }
  ntu = sum / rep;
  Serial.print("Turbidez (NTU): ");
  Serial.println(ntu);
  return ntu;
}

// Función para leer el sensor de TDS
float readTDSSensor(float temperature) {
  gravityTds.setTemperature(temperature);
  gravityTds.update();
  float tdsValue = gravityTds.getTdsValue();
  Serial.print("TDS: ");
  Serial.println(tdsValue);
  return tdsValue;
}

// Función para leer el sensor de Oxígeno Disuelto (OD)
float readDissolvedOxygen() {
  // Enviar comando "R" al sensor de OD para solicitar lectura
  ODSensorSerial.print("R\r");

  // Esperar respuesta con timeout
  unsigned long timeout = millis() + 1000; // Timeout de 1 segundo
  String sensorString = "";

  while (millis() < timeout) {
    if (ODSensorSerial.available()) {
      char c = ODSensorSerial.read();
      if (c == '\r') {
        break;
      }
      sensorString += c;
    }
  }

  if (sensorString.length() > 0) {
    float DO = sensorString.toFloat();
    Serial.print("Oxígeno Disuelto (%): ");
    Serial.println(DO);
    return DO;
  } else {
    Serial.println("Error: No se recibió lectura de OD");
    return -1.0; // Indica un error
  }
}

// Función para construir el JSON
String buildJSON(bool isAlert) {
  String jsonData = "{";
  jsonData += "\"deviceSerialNumber\":\"" + String(deviceSerialNumber) + "\",";
  
  // Timestamp basado en millis() (No es preciso)
  unsigned long currentMillis = millis();
  unsigned long seconds = (currentMillis / 1000) % 60;
  unsigned long minutes = (currentMillis / 60000) % 60;
  unsigned long hours = (currentMillis / 3600000) % 24;
  String timestamp = "2024-04-27T" + 
                     String(hours) + ":" + 
                     String(minutes) + ":" + 
                     String(seconds) + "Z";
  jsonData += "\"timestamp\":\"" + timestamp + "\",";
  
  jsonData += "\"measurements\":[";
  
  // pH
  jsonData += "{";
  jsonData += "\"sensorSerialNumber\":\"" + String(sensorSerialNumbers[0]) + "\",";
  jsonData += "\"parameterName\":\"" + String(parameterNames[0]) + "\",";
  jsonData += "\"value\":" + String(sumPH / readingCount, 2) + ",";
  jsonData += "\"alert\":" + String(alertPH ? "true" : "false");
  jsonData += "},";
  
  // Temperatura
  jsonData += "{";
  jsonData += "\"sensorSerialNumber\":\"" + String(sensorSerialNumbers[1]) + "\",";
  jsonData += "\"parameterName\":\"" + String(parameterNames[1]) + "\",";
  jsonData += "\"value\":" + String(sumTemperature / readingCount, 2) + ",";
  jsonData += "\"alert\":" + String(alertTemp ? "true" : "false");
  jsonData += "},";
  
  // TDS
  jsonData += "{";
  jsonData += "\"sensorSerialNumber\":\"" + String(sensorSerialNumbers[2]) + "\",";
  jsonData += "\"parameterName\":\"" + String(parameterNames[2]) + "\",";
  jsonData += "\"value\":" + String(sumTDS / readingCount, 2) + ",";
  jsonData += "\"alert\":" + String(alertTDS ? "true" : "false");
  jsonData += "},";
  
  // Oxígeno Disuelto
  jsonData += "{";
  jsonData += "\"sensorSerialNumber\":\"" + String(sensorSerialNumbers[3]) + "\",";
  jsonData += "\"parameterName\":\"" + String(parameterNames[3]) + "\",";
  jsonData += "\"value\":" + String(sumDO / readingCount, 2) + ",";
  jsonData += "\"alert\":" + String(alertDO ? "true" : "false");
  jsonData += "},";
  
  // Turbidez
  jsonData += "{";
  jsonData += "\"sensorSerialNumber\":\"" + String(sensorSerialNumbers[4]) + "\",";
  jsonData += "\"parameterName\":\"" + String(parameterNames[4]) + "\",";
  jsonData += "\"value\":" + String(sumTurbidity / readingCount, 2) + ",";
  jsonData += "\"alert\":" + String(alertTurbidity ? "true" : "false");
  jsonData += "}";
  
  jsonData += "]}";
  
  return jsonData;
}

// Función para enviar el JSON al ESP8266-01
void sendJSON(String jsonData) {
  espSerial.println(jsonData); // Enviar el JSON a través de Serial1
  Serial.println("Enviando datos al ESP8266-01:");
  Serial.println(jsonData);
}
