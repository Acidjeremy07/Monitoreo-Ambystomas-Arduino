#include <OneWire.h>
#include <DallasTemperature.h>
#include <GravityTDS.h>
#include <EEPROM.h>
#include <ArduinoJson.h>  // Necesario para parsear JSON

// Pines de conexión
#define TdsSensorPin A1      // Total de Sólidos Disueltos (TDS)
#define PHPin A0             // Sensor de pH
#define TempSensorPin 23     // Sensor de Temperatura (OneWire)
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

// Variables para control de mediciones
bool shouldMeasure = true; // Controla si se deben realizar mediciones
bool takeImmediateMeasurement = false; // Controla si se debe tomar una medición inmediata

// Variables para temporización
unsigned long lastMeasurementTime = 0;
const unsigned long measurementInterval = 1000; // 1 minuto en milisegundos

// Variable para el modo de calibración TDS
bool tdsCalibrationMode = false;
float tdsCalibrationFactor = 1.0; // Factor de calibración para TDS

// Funciones Prototipo
float readTemperatureSensor();
float readPHSensor();
float readTurbiditySensor();
float readTDSSensor(float temperature);
float readDissolvedOxygen();
String buildJSON(bool isAlert);
void sendJSON(String jsonData);
void checkForCommands();
void processCommand(String commandData, bool fromSerialMonitor);
void calibratePH();
void calibrateDO();
void calibrateTDS(float calibrationValue);
void takeMeasurement();
void calibrate(float calibrationPH); // Función para calibrar pH

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
  gravityTds.setKvalueAddress(0x08); // Establecer la dirección en EEPROM para almacenar el valor K
  gravityTds.begin();

  // Leer factor de calibración de TDS desde EEPROM
  EEPROM.get(10, tdsCalibrationFactor);
  if (isnan(tdsCalibrationFactor) || tdsCalibrationFactor == 0.0) {
    tdsCalibrationFactor = 1.0; // Valor por defecto
    Serial.println("No se encontró factor de calibración de TDS en EEPROM. Usando valor por defecto.");
  } else {
    Serial.print("Factor de calibración de TDS leído de EEPROM: ");
    Serial.println(tdsCalibrationFactor, 4);
  }

  // Esperar a que los sensores se inicien
  delay(2000);

  Serial.println("Sistema iniciado. Esperando comandos o iniciando mediciones...");
}

void loop() {
  // Revisar si hay comandos entrantes
  checkForCommands();

  if (shouldMeasure) {
    unsigned long currentMillis = millis();

    // Leer sensores cada minuto
    if (currentMillis - lastMeasurementTime >= measurementInterval) {
      lastMeasurementTime = currentMillis;
      takeMeasurement();
    }
  }

  if (takeImmediateMeasurement) {
    takeImmediateMeasurement = false;
    takeMeasurement();
  }
}

// Función para leer el sensor de temperatura
float readTemperatureSensor() {
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
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
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
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
  for (int i = 0; i < rep; i++) {
    sensorValue = analogRead(TurbidityPin);
    float voltage = sensorValue * (5.0 / 1024.0);
    tempA25 = voltCal - varTemp;
    K = -865.68 * tempA25;
    ntu = (-865.68 * voltage) - K;
    if (ntu < 0) {
      ntu = 0;
    }
    sum += ntu;
    delay(10);
  }
  ntu = sum / rep;
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.print("Turbidez (NTU): ");
  Serial.println(ntu);
  return ntu;
}

// Función para leer el sensor de TDS
float readTDSSensor(float temperature) {
  gravityTds.setTemperature(temperature);
  gravityTds.update();
  float tdsValue = gravityTds.getTdsValue();
  tdsValue = tdsValue * tdsCalibrationFactor; // Aplicar factor de calibración
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.print("TDS: ");
  Serial.print(tdsValue, 0);
  Serial.println(" ppm");
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
    Serial.print("[");
    Serial.print(millis());
    Serial.print(" ms] ");
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

// Función para enviar el JSON al ESP8266-01 y también imprimir en el Serial Monitor
void sendJSON(String jsonData) {
  espSerial.println(jsonData); // Enviar el JSON a través de Serial1
  Serial.println("Enviando datos al ESP8266-01:");
  Serial.println(jsonData);
  Serial.println("Datos enviados al ESP8266-01.");

  // También podemos imprimir el JSON formateado en el Serial Monitor para mayor claridad
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonData);

  if (error) {
    Serial.print("Error al deserializar JSON: ");
    Serial.println(error.c_str());
  } else {
    Serial.println("JSON formateado:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  }
}

// Función para revisar y procesar comandos entrantes
void checkForCommands() {
  // Comandos desde el ESP8266-01
  if (espSerial.available() > 0) {
    String commandData = espSerial.readStringUntil('\n'); // Lee hasta el fin de línea
    processCommand(commandData, false); // false indica que no es desde el Monitor Serial
  }

  // Comandos desde el Monitor Serial
  if (Serial.available() > 0) {
    String commandData = Serial.readStringUntil('\n'); // Lee hasta el fin de línea
    processCommand(commandData, true); // true indica que es desde el Monitor Serial
  }
}

// Función para procesar comandos
void processCommand(String commandData, bool fromSerialMonitor) {
  commandData.trim(); // Eliminar espacios en blanco

  // Si estamos en modo de calibración TDS
  if (tdsCalibrationMode) {
    // Manejar comandos de calibración
    if (commandData.equalsIgnoreCase("cal:500")) {
      // Calibrar a 500ppm
      calibrateTDS(500.0); // Pasar el valor de calibración
    } else if (commandData.equalsIgnoreCase("exit")) {
      // Salir del modo de calibración
      tdsCalibrationMode = false;
      Serial.println("Saliendo del modo de calibración TDS.");
    } else {
      Serial.println("Comando desconocido en modo de calibración TDS.");
    }
    return; // Salir de la función
  }

  // No estamos en modo de calibración TDS
  // Intentar parsear como JSON
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, commandData);

  if (!error) {
    // JSON parseado exitosamente
    // Extraer datos
    const char* deviceSerial = doc["deviceSerialNumber"];
    const char* commandType = doc["commandType"];
    const char* sensorType = doc["sensorType"]; // Opcional para calibración
    const char* timestamp = doc["timestamp"]; // Opcional

    Serial.print("Comando JSON recibido: ");
    Serial.println(commandType);

    // Procesar el comando
    if (strcmp(commandType, "Stop") == 0) {
      shouldMeasure = false;
      Serial.println("Deteniendo mediciones.");
    } else if (strcmp(commandType, "Start") == 0) {
      shouldMeasure = true;
      Serial.println("Iniciando mediciones.");
    } else if (strcmp(commandType, "Calibrate") == 0) {
      if (sensorType != nullptr) {
        if (strcmp(sensorType, "pH") == 0) {
          Serial.println("Iniciando calibración del sensor de pH.");
          calibratePH();
        } else if (strcmp(sensorType, "DO") == 0) {
          Serial.println("Iniciando calibración del sensor de Oxígeno Disuelto.");
          calibrateDO();
        } else if (strcmp(sensorType, "TDS") == 0) {
          Serial.println("Entrando en modo de calibración TDS.");
          tdsCalibrationMode = true;
          Serial.println("Ingrese 'cal:500' para calibrar a 500ppm.");
          Serial.println("Ingrese 'exit' para salir del modo de calibración.");
        } else {
          Serial.println("Tipo de sensor desconocido para calibración.");
        }
      } else {
        Serial.println("No se especificó el tipo de sensor para calibrar.");
      }
    } else if (strcmp(commandType, "Measure") == 0) {
      Serial.println("Realizando medición inmediata.");
      takeImmediateMeasurement = true;
    } else {
      Serial.println("Comando desconocido.");
    }

  } else {
    // No es JSON, tratar como comando de texto
    Serial.print("Comando de texto recibido: ");
    Serial.println(commandData);

    if (commandData.equalsIgnoreCase("Start")) {
      shouldMeasure = true;
      Serial.println("Iniciando mediciones.");
    } else if (commandData.equalsIgnoreCase("Stop")) {
      shouldMeasure = false;
      Serial.println("Deteniendo mediciones.");
    } else if (commandData.equalsIgnoreCase("Measure")) {
      takeImmediateMeasurement = true;
      Serial.println("Realizando medición inmediata.");
    } else if (commandData.equalsIgnoreCase("enter")) {
      // Entrar en modo de calibración TDS
      tdsCalibrationMode = true;
      Serial.println("Entrando en modo de calibración TDS.");
      Serial.println("Ingrese 'cal:500' para calibrar a 500ppm.");
      Serial.println("Ingrese 'exit' para salir del modo de calibración.");
    } else {
      Serial.println("Comando desconocido.");
    }
  }
}

// Función para calibrar el sensor de pH
void calibratePH() {
  float calibrationPH = 6.86; // Valor de la solución de calibración
  Serial.println("Calibrando sensor de pH...");
  calibrate(calibrationPH);
}

// Función para calibrar el sensor de Oxígeno Disuelto (DO)
void calibrateDO() {
  Serial.println("Calibrando sensor de Oxígeno Disuelto...");
  // Enviar comando "Cal" al sensor de OD para calibración en aire
  ODSensorSerial.print("Cal\r");

  // Esperar respuesta con timeout
  unsigned long timeout = millis() + 1000; // Timeout de 1 segundo
  String response = "";

  while (millis() < timeout) {
    if (ODSensorSerial.available()) {
      char c = ODSensorSerial.read();
      if (c == '\r') {
        break;
      }
      response += c;
    }
  }

  if (response.length() > 0) {
    Serial.print("Respuesta de calibración del sensor DO: ");
    Serial.println(response);
    // Verificar si la respuesta indica éxito
    if (response.indexOf("*OK") != -1) {
      Serial.println("Calibración de Oxígeno Disuelto exitosa.");
    } else {
      Serial.println("Error en calibración de Oxígeno Disuelto.");
    }
  } else {
    Serial.println("No se recibió respuesta del sensor DO para calibración.");
  }
}

// Función para calibrar el sensor de TDS
void calibrateTDS(float calibrationValue) {
  Serial.print("Calibrando sensor TDS a ");
  Serial.print(calibrationValue);
  Serial.println(" ppm.");

  // Asegurar que la temperatura es 25°C
  gravityTds.setTemperature(25.0);
  gravityTds.update();
  float tdsCal = gravityTds.getTdsValue();

  // Calcular factor de calibración
  tdsCalibrationFactor = calibrationValue / tdsCal;

  Serial.print("Factor de calibración calculado: ");
  Serial.println(tdsCalibrationFactor, 4);

  // Guardar factor de calibración en EEPROM
  EEPROM.put(10, tdsCalibrationFactor); // Guarda en dirección 10
  Serial.println("Factor de calibración guardado en EEPROM.");
}

// Función para tomar una medición
void takeMeasurement() {
  Serial.println("Iniciando medición...");
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

// Función para calibrar el sensor de pH con un valor dado
void calibrate(float calibrationPH) {
  float sumVoltage = 0;
  for (int i = 0; i < 10; i++) {
    sumVoltage += analogRead(PHPin) * (5.0 / 1024.0);
    delay(10);
  }
  float avgVoltage = sumVoltage / 10.0;
  // Ajustar 'b' basado en el nuevo pH y voltaje medio
  b = calibrationPH - m * avgVoltage;
  Serial.print("Calibrado a pH: ");
  Serial.print(calibrationPH, 2);
  Serial.print(" con voltaje: ");
  Serial.println(avgVoltage, 2);
}
