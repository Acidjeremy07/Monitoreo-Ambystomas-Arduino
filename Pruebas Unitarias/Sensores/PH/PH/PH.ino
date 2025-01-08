#include <EEPROM.h>

// Definición de pines y variables
const int PHPin = A0;  // Sensor conectado al PIN A0

// Variables de calibración
float m = -5.0;    // Pendiente inicial (puede ajustarse después de la calibración)
float b = 15.0;    // Intersección inicial (puede ajustarse después de la calibración)

// Direcciones de EEPROM para almacenar m y b
const int EEPROM_ADDRESS_M = 0; // Direcciones 0-3
const int EEPROM_ADDRESS_B = 4; // Direcciones 4-7

// Flag para iniciar/detener mediciones
bool shouldMeasure = false;

// Variables de calibración temporales
float calibrationPH1 = 0.0;
float calibrationPH2 = 0.0;
float voltage1 = 0.0;
float voltage2 = 0.0;
bool point1Calibrated = false;
bool point2Calibrated = false;

void setup() {
  Serial.begin(115200);  // Iniciar comunicación serial a 115200 baudios

  // Leer valores de calibración de la EEPROM
  EEPROM.get(EEPROM_ADDRESS_M, m);
  EEPROM.get(EEPROM_ADDRESS_B, b);

  Serial.println("=== Inicialización Completa ===");
  Serial.print("m (Pendiente): ");
  Serial.println(m, 4);
  Serial.print("b (Intersección): ");
  Serial.println(b, 4);
  Serial.println("Listo para recibir comandos.");
}

void loop() {
  if (shouldMeasure) {
    PHpromedio();  // Función que calcula el pH promedio de 10 muestras
    delay(1000);   // Medición cada segundo
  }

  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');   // Leer comando hasta el salto de línea
    command.trim(); // Eliminar espacios al inicio y al final
    Serial.print("Comando recibido: ");
    Serial.println(command);
    handleInput(command);
  }
}

void handleInput(String input) {
  if (input.startsWith("calibrate ")) {
    String calibrationType = input.substring(10); // Extraer después de "calibrate "
    calibrationType.trim(); // Eliminar espacios adicionales

    if (calibrationType.startsWith("point1 ")) {
      String phValueStr = calibrationType.substring(7); // Extraer después de "point1 "
      phValueStr.trim();
      float calibrationPH = phValueStr.toFloat();
      if (calibrationPH <= 0.0 || calibrationPH >= 14.0) {
        Serial.println("Error: pH inválido. Debe estar entre 0 y 14.");
        return;
      }
      calibratePoint1(calibrationPH);
    }
    else if (calibrationType.startsWith("point2 ")) {
      String phValueStr = calibrationType.substring(7); // Extraer después de "point2 "
      phValueStr.trim();
      float calibrationPH = phValueStr.toFloat();
      if (calibrationPH <= 0.0 || calibrationPH >= 14.0) {
        Serial.println("Error: pH inválido. Debe estar entre 0 y 14.");
        return;
      }
      calibratePoint2(calibrationPH);
    }
    else {
      Serial.println("Formato de calibración incorrecto. Usa 'calibrate point1 <pH>' o 'calibrate point2 <pH>'");
    }
  }
  else if (input.equals("save")) {
    saveCalibration();
  }
  else if (input.equals("start")) {
    Serial.println("Iniciando medición...");
    shouldMeasure = true;
  }
  else if (input.equals("stop")) {
    Serial.println("Deteniendo medición...");
    shouldMeasure = false;
  }
  else if (input.equals("status")) {
    Serial.print("Estado de Medición: ");
    Serial.println(shouldMeasure ? "Activado" : "Desactivado");
    Serial.print("m (Pendiente): ");
    Serial.println(m, 4);
    Serial.print("b (Intersección): ");
    Serial.println(b, 4);
    Serial.print("Punto 1 Calibrado: ");
    Serial.println(point1Calibrated ? "Sí" : "No");
    if (point1Calibrated) {
      Serial.print("pH1: ");
      Serial.println(calibrationPH1, 2);
      Serial.print("Voltaje1: ");
      Serial.println(voltage1, 4);
    }
    Serial.print("Punto 2 Calibrado: ");
    Serial.println(point2Calibrated ? "Sí" : "No");
    if (point2Calibrated) {
      Serial.print("pH2: ");
      Serial.println(calibrationPH2, 2);
      Serial.print("Voltaje2: ");
      Serial.println(voltage2, 4);
    }
  }
  else if (input.equals("reset")) {
    resetCalibration();
  }
  else {
    Serial.println("Comando no reconocido.");
  }
}

void calibratePoint1(float calibrationPH) {
  Serial.println("Calibrando Punto 1...");
  voltage1 = readAverageVoltage();
  calibrationPH1 = calibrationPH;
  point1Calibrated = true;
  Serial.print("Punto 1 calibrado: pH = ");
  Serial.print(calibrationPH1, 2);
  Serial.print(", Voltaje = ");
  Serial.println(voltage1, 4);
  
  if (point1Calibrated && point2Calibrated) {
    calculateCalibration();
  }
}

void calibratePoint2(float calibrationPH) {
  Serial.println("Calibrando Punto 2...");
  voltage2 = readAverageVoltage();
  calibrationPH2 = calibrationPH;
  point2Calibrated = true;
  Serial.print("Punto 2 calibrado: pH = ");
  Serial.print(calibrationPH2, 2);
  Serial.print(", Voltaje = ");
  Serial.println(voltage2, 4);
  
  if (point1Calibrated && point2Calibrated) {
    calculateCalibration();
  }
}

void calculateCalibration() {
  if (voltage2 == voltage1) {
    Serial.println("Error: Voltajes de calibración iguales. No se puede calcular la pendiente.");
    return;
  }

  // Y = mX + b
  // Tenemos dos puntos: (V1, pH1) y (V2, pH2)
  m = (calibrationPH2 - calibrationPH1) / (voltage2 - voltage1);
  b = calibrationPH1 - m * voltage1;

  Serial.println("=== Calibración Completada ===");
  Serial.print("m (Pendiente): ");
  Serial.println(m, 4);
  Serial.print("b (Intersección): ");
  Serial.println(b, 4);
  Serial.println("Usa el comando 'save' para almacenar la calibración.");
}

void saveCalibration() {
  EEPROM.put(EEPROM_ADDRESS_M, m);
  EEPROM.put(EEPROM_ADDRESS_B, b);
  Serial.println("Calibración guardada en EEPROM.");
}

float readAverageVoltage() {
  long sum = 0;
  const int numSamples = 10;
  for (int i = 0; i < numSamples; i++) {
    int analogValue = analogRead(PHPin);
    sum += analogValue;
    delay(10); // Espera de 10 ms entre lecturas
  }
  float avgAnalog = sum / (float)numSamples;
  float voltage = avgAnalog * (5.0 / 1024.0);
  return voltage;
}

void PHpromedio() {
  float phVoltaje = readAverageVoltage();
  float pHValor = m * phVoltaje + b;
  impresion(pHValor, phVoltaje);
}

void impresion(float ph, float v) {
  Serial.print("pH Promedio: ");
  Serial.print(ph, 2);  // Mostrar pH con dos decimales
  Serial.print("  Voltaje Promedio: ");
  Serial.println(v, 4); // Mostrar voltaje con cuatro decimales
}

void resetCalibration() {
  point1Calibrated = false;
  point2Calibrated = false;
  calibrationPH1 = 0.0;
  calibrationPH2 = 0.0;
  voltage1 = 0.0;
  voltage2 = 0.0;
  m = -5.0;    // Restablecer a valores iniciales
  b = 15.0;
  Serial.println("=== Calibración Reiniciada ===");
}
