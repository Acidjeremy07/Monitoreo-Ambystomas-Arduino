#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "GravityTDS.h"
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <SoftwareSerial.h>
#include <math.h> // Incluir math.h para funciones matemáticas

// Configuración de WiFi
const char* ssid = "Jeremy";
const char* password = "123456789";

// Configuración del servidor
const char* serverIP = "192.168.189.180";
const uint16_t serverPort = 8080;

// Pines de conexión
#define TdsSensorPin A1      // TDS
#define PHPin A0             // pH
#define TempSensorPin 4      // Temperatura OneWire (cambiado de 2 a 4)
#define TurbidityPin A3      // Turbidez

// Pines para SoftwareSerial del sensor de OD
#define OD_RX_PIN 2          // RX del sensor OD conectado al pin digital 2
#define OD_TX_PIN 3          // TX del sensor OD conectado al pin digital 3

// Inicializar SoftwareSerial para el sensor de OD
SoftwareSerial odSerial(OD_RX_PIN, OD_TX_PIN); // RX, TX

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
const float minPH = 6.00; // Umbral mínimo de pH
const float maxPH = 8.00; // Umbral máximo de pH
const float minTemp = 14.00;
const float maxTemp = 20.00;
const float minDO = 5.9; 
const float maxTDS = 1300.0; 
const float maxTurbidity = 15.00; 

// Variables globales para sensores
GravityTDS gravityTds;
OneWire oneWire(TempSensorPin);
DallasTemperature sensors(&oneWire);

float temperature = 25.0;
float tdsValue = 0.0;

// Constantes fijas para pH (sin calibración)
// Asegúrate de que estos valores sean correctos para tu sensor
const float m = -14.3;    // Pendiente de la recta (Y = mX + b)
const float b = 64.346;   // Intersección de la recta (Y = mX + b)

// Variables para promediar datos
const unsigned long sendInterval = 60000; // 10 minutos
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

// Control de mediciones
bool shouldMeasure = true;
bool takeImmediateMeasurement = false;

// Intervalos de medición
unsigned long lastMeasurementTime = 0;
const unsigned long measurementInterval = 1000; // 1 seg

// Alertas
bool alertSent = false;
unsigned long alertSentTime = 0;
const unsigned long alertRetryInterval = 180000; // 30 seg

// Tras una alerta, esperar 30 seg antes de volver a medir
bool waitAfterAlert = false;
unsigned long waitStartTime = 0;

// Modo calibración (solo para TDS y DO)
enum CalibrationMode { NONE, CAL_TDS, CAL_DO };
CalibrationMode calibrationMode = NONE;

// Factor calibración TDS
float tdsCalibrationFactor = 1.0;

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// WiFiClient para comunicación con el servidor
WiFiClient client;

// Funciones prototipo
float readTemperatureSensor();
float readPHSensor();
float readTurbiditySensor();
float readTDSSensor(float temperature);
float readDissolvedOxygen();
String buildJSON(bool isAlert);
void sendJSON(String jsonData);
void checkForCommands();
void processCommand(String commandData, bool fromSerialMonitor);
void calibrateDO();
void calibrateTDS(float calibrationValue);
void takeMeasurement();
void handleCalibration();
void handleAlertRetry();

// Función de setup
void setup() {
  // Iniciar comunicación serial para depuración
  Serial.begin(115200);
  delay(2000);

  // Iniciar SoftwareSerial para el sensor de OD
  odSerial.begin(9600);
  Serial.println("SoftwareSerial para OD iniciado en pines 2 (RX) y 3 (TX).");

  // Iniciar WiFi
  WiFi.begin(ssid, password);
  Serial.println("Conectando a WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado a WiFi: " + WiFi.localIP().toString());

  timeClient.begin();
  timeClient.update();

  sensors.begin();
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);
  gravityTds.setAdcRange(1024);
  gravityTds.setKvalueAddress(0x08);
  gravityTds.begin();

  // Leer factor calibración TDS desde EEPROM
  EEPROM.get(10, tdsCalibrationFactor);
  if (isnan(tdsCalibrationFactor) || tdsCalibrationFactor == 0.0) {
    tdsCalibrationFactor = 1.0;
    Serial.println("No factor de calibración TDS en EEPROM. Usando valor por defecto.");
  } else {
    Serial.print("Factor calibración TDS: ");
    Serial.println(tdsCalibrationFactor, 4);
  }

  // Realizar primera medición inmediata
  takeImmediateMeasurement = true;

  Serial.println("Sistema iniciado. Esperando comandos...");
}

// Función de loop
void loop() {
  timeClient.update();
  checkForCommands();
  handleAlertRetry();

  // Si se ha recibido un comando "stop", no medir hasta "start" o "measure"
  if (!shouldMeasure) {
    return; // No medir ni hacer nada de sensores
  }

  // Si estamos esperando 30 seg después de alerta
  if (waitAfterAlert) {
    if (millis() - waitStartTime < 30000) {
      return; // Esperar 30 seg sin medir
    } else {
      waitAfterAlert = false;
    }
  }

  if (calibrationMode == NONE) {
    unsigned long currentMillis = millis();
    if (takeImmediateMeasurement) {
      takeImmediateMeasurement = false;
      takeMeasurement();
    } else if (currentMillis - lastMeasurementTime >= measurementInterval) {
      lastMeasurementTime = currentMillis;
      takeMeasurement();
    }
  } else {
    handleCalibration();
  }
}

// Funciones de lectura de sensores
float readTemperatureSensor() {
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  Serial.print("Temperatura: ");
  Serial.println(temp);
  return temp;
}

float readPHSensor() {
  long samples = 0;
  const int numSamples = 20; // Aumentar el número de muestras para mayor precisión
  for (int x = 0; x < numSamples; x++) {
    samples += analogRead(PHPin);
    delay(10);
  }
  float phVoltage = (samples / (float)numSamples) * (5.0 / 1024.0);
  float pHValue = m * phVoltage + b;
  Serial.print("pH: ");
  Serial.println(pHValue);
  return pHValue;
}

float readTurbiditySensor() {
  float voltCal = 1.10;   
  float varTemp = 0.0;
  float tempA25 = 0.0;
  float K = 0.0;
  float ntu = 0.0;
  int sensorValue = 0;

  float sumVal = 0.0;
  int rep = 30;
  for (int i = 0; i < rep; i++) {
    sensorValue = analogRead(TurbidityPin);
    float voltage = sensorValue * (5.0 / 1024.0);
    tempA25 = voltCal - varTemp;
    K = -865.68 * tempA25;
    ntu = (-865.68 * voltage) - K;
    if (ntu < 0) ntu = 0;
    sumVal += ntu;
    delay(10);
  }
  ntu = sumVal / rep;
  Serial.print("Turbidez (NTU): ");
  Serial.println(ntu);
  return ntu;
}

float readTDSSensor(float temperature) {
  gravityTds.setTemperature(temperature);
  gravityTds.update();
  float val = gravityTds.getTdsValue() * tdsCalibrationFactor;
  Serial.print("TDS: ");
  Serial.print(val, 0);
  Serial.println(" ppm");
  return val;
}

// Función simulada de lectura de DO (Onda Senoidal)
float readDissolvedOxygen() {
  static unsigned long lastUpdate = 0;
  static float angle = 0.0;
  unsigned long currentMillis = millis();
  
  // Actualizar el ángulo cada segundo
  if (currentMillis - lastUpdate >= 1000) { // 1 segundo
    angle += 0.05; // Velocidad de variación
    if (angle > 2 * PI) angle -= 2 * PI;
    lastUpdate = currentMillis;
  }
  
  // Mapear la onda senoidal (-1 a 1) al rango deseado (6.0 a 8.3)
  float DO = 7.15 + 1.15 * sin(angle); // Promedio 7.15, amplitud 1.15
  Serial.print("Simulated DO (mg/L): ");
  Serial.println(DO);
  return DO;
}

/*
// Alternativamente, si prefieres usar valores aleatorios, comenta la función anterior y usa la siguiente:
float readDissolvedOxygen() {
  // Inicializar la semilla aleatoria una vez
  static bool seedInitialized = false;
  if (!seedInitialized) {
    randomSeed(analogRead(0)); // Usar un pin analógico no conectado para la semilla
    seedInitialized = true;
  }

  // Generar un valor aleatorio entre 6.0 y 8.3
  float DO = 6.0 + ((float)rand() / RAND_MAX) * (8.3 - 6.0);
  Serial.print("Simulated DO (mg/L): ");
  Serial.println(DO);
  return DO;
}
*/

// Construir JSON
String buildJSON(bool isAlert) {
  StaticJsonDocument<1024> doc;
  doc["deviceSerialNumber"] = deviceSerialNumber;

  // Timestamp
  unsigned long epochTime = timeClient.getEpochTime();
  time_t rawTime = epochTime;
  struct tm * ti = gmtime(&rawTime);
  char timestamp[25];
  sprintf(timestamp, "%04d-%02d-%02dT%02d:%02d:%02dZ", ti->tm_year + 1900, ti->tm_mon + 1,
          ti->tm_mday, ti->tm_hour, ti->tm_min, ti->tm_sec);
  doc["timestamp"] = timestamp;

  JsonArray measurements = doc.createNestedArray("measurements");

  // pH
  JsonObject phObj = measurements.createNestedObject();
  phObj["sensorSerialNumber"] = sensorSerialNumbers[0];
  phObj["parameterName"] = parameterNames[0];
  phObj["value"] = sumPH / readingCount;
  phObj["alert"] = alertPH;

  // Temp
  JsonObject tempObj = measurements.createNestedObject();
  tempObj["sensorSerialNumber"] = sensorSerialNumbers[1];
  tempObj["parameterName"] = parameterNames[1];
  tempObj["value"] = sumTemperature / readingCount;
  tempObj["alert"] = alertTemp;

  // TDS
  JsonObject tdsObj = measurements.createNestedObject();
  tdsObj["sensorSerialNumber"] = sensorSerialNumbers[2];
  tdsObj["parameterName"] = parameterNames[2];
  tdsObj["value"] = sumTDS / readingCount;
  tdsObj["alert"] = alertTDS;

  // DO
  JsonObject doObj = measurements.createNestedObject();
  doObj["sensorSerialNumber"] = sensorSerialNumbers[3];
  doObj["parameterName"] = parameterNames[3];
  doObj["value"] = sumDO / readingCount;
  doObj["alert"] = alertDO;

  // Turbidez
  JsonObject turbObj = measurements.createNestedObject();
  turbObj["sensorSerialNumber"] = sensorSerialNumbers[4];
  turbObj["parameterName"] = parameterNames[4];
  turbObj["value"] = sumTurbidity / readingCount;
  turbObj["alert"] = alertTurbidity;

  String jsonData;
  serializeJson(doc, jsonData);
  return jsonData;
}

// Enviar JSON al servidor
void sendJSON(String jsonData) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connect(serverIP, serverPort)) {
      Serial.println("No se pudo conectar al servidor");
      return;
    }

    // Imprimir JSON en monitor serial antes de enviar
    Serial.println("JSON a enviar:");
    StaticJsonDocument<1024> debugDoc;
    DeserializationError error = deserializeJson(debugDoc, jsonData);
    if (!error) {
      serializeJsonPretty(debugDoc, Serial);
    } else {
      Serial.println("Error al deserializar para debug.");
      Serial.println(jsonData);
    }

    // Enviar el JSON
    client.println(jsonData);

    // Esperar respuesta (opcional)
    unsigned long t = millis() + 2000;
    while (millis() < t && client.connected()) {
      if (client.available()) {
        String response = client.readStringUntil('\n');
        Serial.print("Resp servidor: ");
        Serial.println(response);
        break;
      }
    }

    client.stop();
  } else {
    Serial.println("No conectado a WiFi");
  }
}

// Revisar y procesar comandos desde Serial
void checkForCommands() {
  if (Serial.available() > 0) {
    String commandData = Serial.readStringUntil('\n');
    processCommand(commandData, true);
  }
}

// Procesar comando
void processCommand(String commandData, bool fromSerialMonitor) {
  commandData.trim();

  // Si calibración en curso
  if (calibrationMode != NONE && fromSerialMonitor) {
    if (calibrationMode == CAL_TDS) {
      if (commandData.startsWith("cal:")) {
        float calValue = commandData.substring(4).toFloat();
        if (calValue > 0) calibrateTDS(calValue);
        else Serial.println("Valor calibración TDS inválido.");
      } else if (commandData.equalsIgnoreCase("exit")) {
        calibrationMode = NONE;
        Serial.println("Saliendo calibración TDS.");
      } else {
        Serial.println("Comando desconocido calibración TDS.");
      }
      return;
    } else if (calibrationMode == CAL_DO) {
      Serial.println("Calibración DO en proceso...");
      calibrationMode = NONE;
      return;
    }
  }

  // Comandos generales
  if (commandData.equalsIgnoreCase("Start")) {
    shouldMeasure = true;
    Serial.println("Iniciando mediciones.");
  } else if (commandData.equalsIgnoreCase("Stop")) {
    shouldMeasure = false;
    Serial.println("Deteniendo mediciones.");
  } else if (commandData.equalsIgnoreCase("Measure")) {
    shouldMeasure = true;
    takeImmediateMeasurement = true;
    Serial.println("Medición inmediata.");
  } else if (commandData.equalsIgnoreCase("calibrate")) {
    // Solo permitir calibración de TDS y DO
    Serial.println("Tipo de calibración: TDS, DO.");
    // Aquí podrías implementar opciones de calibración si es necesario
  } else if (commandData.equalsIgnoreCase("TDS") && calibrationMode == NONE) {
    if (fromSerialMonitor) {
      calibrationMode = CAL_TDS;
      Serial.println("Calibración TDS. 'cal:<valor>' o 'exit'.");
    } else {
      Serial.println("Comando 'TDS' requiere interacción manual.");
    }
  } else if (commandData.equalsIgnoreCase("DO") && calibrationMode == NONE) {
    if (fromSerialMonitor) {
      calibrateDO();
    } else {
      Serial.println("Comando 'DO' requiere interacción manual.");
    }
  } else if (commandData.equalsIgnoreCase("save")) {
    // No hay calibración pH que guardar
    Serial.println("No hay calibración para guardar.");
  } else {
    Serial.println("Comando desconocido.");
  }
}

// Calibrar DO
void calibrateDO() {
  Serial.println("Calibrando DO con aire...");
  calibrationMode = CAL_DO;
  shouldMeasure = false;

  // Enviar comando calibración OD a través de SoftwareSerial (adaptar según sensor)
  odSerial.print("Cal\r");
  unsigned long t = millis() + 2000;
  String response = "";
  while (millis() < t) {
    if (odSerial.available()) {
      char c = odSerial.read();
      if (c == '\r') break;
      response += c;
    }
  }

  if (response.length() > 0) {
    Serial.print("Resp calibración DO: ");
    Serial.println(response);
    if (response.indexOf("*OK") != -1 || response.indexOf("OK") != -1) {
      Serial.println("Calibración DO exitosa.");
    } else {
      Serial.println("Error calibración DO.");
    }
  } else {
    Serial.println("No respuesta sensor DO.");
  }

  calibrationMode = NONE;
}

// Calibrar TDS
void calibrateTDS(float calibrationValue) {
  Serial.print("Calibrando TDS a ");
  Serial.print(calibrationValue);
  Serial.println(" ppm.");
  gravityTds.setTemperature(25.0);
  gravityTds.update();
  float tdsCal = gravityTds.getTdsValue();
  tdsCalibrationFactor = calibrationValue / tdsCal;
  Serial.print("Factor TDS: ");
  Serial.println(tdsCalibrationFactor, 4);
  EEPROM.put(10, tdsCalibrationFactor);
  Serial.println("Factor TDS guardado.");
  calibrationMode = NONE;
}

// Tomar medición
void takeMeasurement() {
  Serial.println("Tomando medición...");
  temperature = readTemperatureSensor();
  float pH = readPHSensor();
  tdsValue = readTDSSensor(temperature);
  float doValue = readDissolvedOxygen();
  float turbidity = readTurbiditySensor();

  sumTemperature += temperature;
  sumPH += pH;
  sumTDS += tdsValue;
  sumDO += doValue;
  sumTurbidity += turbidity;
  readingCount++;

  bool outOfRange = false;

  // **Desactivar alertas de pH**
  alertPH = false; // Siempre false

  // Actualizar la lógica de alerta para otros parámetros
  alertTemp = (temperature < minTemp || temperature > maxTemp);
  alertDO = (doValue < minDO); // Esta condición nunca será verdadera con la simulación
  alertTurbidity = (turbidity > maxTurbidity);
  alertTDS = (tdsValue > maxTDS);

  // Puedes optar por excluir alertPH de la siguiente línea si lo prefieres:
  // bool outOfRange = alertTemp || alertTDS || alertDO || alertTurbidity;
  
  // O mantener alertPH en la evaluación, pero siempre será false
  outOfRange = alertPH || alertTemp || alertTDS || alertDO || alertTurbidity;

  if (outOfRange) {
    Serial.println("Alerta detectada en parámetros.");
    if (!alertSent) {
      String jsonData = buildJSON(true);
      sendJSON(jsonData);
      alertSent = true;
      alertSentTime = millis();
      // Esperar 30 seg antes de próxima medición
      waitAfterAlert = true;
      waitStartTime = millis();
    }
  } else {
    alertSent = false;
    alertSentTime = 0;
  }

  if (millis() - lastSendTime >= sendInterval) {
    if (readingCount > 0) {
      String jsonData = buildJSON(false);
      sendJSON(jsonData);
      sumTemperature = sumPH = sumTDS = sumDO = sumTurbidity = 0;
      readingCount = 0;
      lastSendTime = millis();
    }
  }
}

// Reintentos de alerta
void handleAlertRetry() {
  if (alertSent && (millis() - alertSentTime >= alertRetryInterval)) {
    Serial.println("Reintentando medición tras alerta...");
    alertSent = false;
    takeImmediateMeasurement = true;
  }
}

// Manejar calibración en proceso (solo TDS y DO)
void handleCalibration() {
  // Actualmente, no hay acciones adicionales necesarias
}
