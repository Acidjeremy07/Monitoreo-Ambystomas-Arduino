const int PHPin = A0;  // Sensor conectado en PIN A0

float b = 38.308;  // Constante de la ecuación (Y = mx + b)
float m = -9.53;  // Pendiente de la recta (Y = mx + b)
bool shouldMeasure = false;  // Flag para comenzar mediciones

void setup() {
  Serial.begin(115200);  // Comunicación con la PC
}

void loop() {
  if (shouldMeasure) {
    PHpromedio();  // función que calcula el ph promedio de 10 muestras
    delay(1000);   // Medición cada segundo
  }

  if (Serial.available() > 0) {                      // Conexión serial para el Arduino IDE
    String command = Serial.readStringUntil('\n');   // Lee el comando del monitor serie
    Serial.print("Comando recibido: ");              // Mensaje de depuración
    Serial.println(command);                         // Mensaje de depuración
    handleInput(command);                            // Maneja el comando
  }
}

void handleInput(String input) {
  input.trim(); // Elimina espacios en blanco al principio y al final

  if (input.startsWith("calibrate ")) {
    float calibrationPH = input.substring(10).toFloat();
    Serial.print("Calibrando a pH: ");               // Mensaje de depuración
    Serial.println(calibrationPH, 2);                // Mensaje de depuración
    calibrate(calibrationPH);
  } else if (input.equals("start")) {
    Serial.println("Iniciando medición...");         // Mensaje de depuración
    shouldMeasure = true;
  } else if (input.equals("stop")) {
    Serial.println("Deteniendo medición...");        // Mensaje de depuración
    shouldMeasure = false;
  } else {
    Serial.println("Comando no reconocido.");        // Mensaje de depuración
  }
}

void calibrate(float calibrationPH) {
  float sumVoltage = 0;
  for (int i = 0; i < 10; i++) {
    sumVoltage += analogRead(PHPin) * (5.0 / 1024.0);
    delay(10);
  }
  float avgVoltage = sumVoltage / 10.0;
  // Suponiendo un pH conocido y un solo punto de calibración
  b = calibrationPH - m * avgVoltage;  // Ajustar 'b' basado en el nuevo pH y voltaje medio
  Serial.print("Calibrado a pH: ");
  Serial.print(calibrationPH, 2);  // Mostrar pH con dos decimales
  Serial.print(" con voltaje: ");
  Serial.println(avgVoltage, 2);   // Mostrar voltaje con dos decimales
}

void PHpromedio() {
  long muestras = 0;
  for (int x = 0; x < 10; x++) {
    muestras += analogRead(PHPin);
    delay(10);
  }
  float phVoltaje = muestras / 10.0 * (5.0 / 1024.0);
  float pHValor = phVoltaje * m + b;
  impresion(pHValor, phVoltaje);
}

void impresion(float ph, float v) {
  Serial.print("Ph: ");
  Serial.print(ph, 2);  // Mostrar pH con dos decimales
  Serial.print("  Voltaje: ");
  Serial.println(v, 2);  // Mostrar voltaje con dos decimales
}
