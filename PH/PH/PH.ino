const int PHPin = A0;  // Sensor conectado en PIN A0

float b = 38.308;  // Constante de la ecuación (Y = mx + b)
float m = -9.53;  // Pendiente de la recta (Y = mx + b)
bool shouldMeasure = false;  // Flag para comenzar mediciones

void setup() {
  Serial.begin(115200);       // Comunicación con la PC
  Serial1.begin(115200);      // Comunicación con el ESP-01
}

void loop() {

  if (shouldMeasure) {
    PHpromedio();  // función que calcula el ph promedio de 10 muestras
    delay(1000);  // Medición cada segundo
  }
  if (Serial.available() > 0) {                       //Conexión serial para el arduino IDE
    handleInput(Serial.readStringUntil('\n'));        //Lee el comando del monitor serie
  }
  if (Serial1.available() > 0) {                      //Conexión serial para el ESP01
    String command = Serial1.readStringUntil('\n');   //Lee el comando del monitor serie que intodujo el monitor serie
    handleInput(command);                             //Llama a la función que lidiara con el comando
  }
}

void handleInput(String input) {
  if (input.startsWith("calibrate ")) {
    float calibrationPH = input.substring(10).toFloat();
    calibrate(calibrationPH);
  } else if (input.startsWith("start")) {
    shouldMeasure = true;
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
  Serial.print(calibrationPH);
  Serial.print(" con voltaje: ");
  Serial.println(avgVoltage);
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
  Serial.print(ph);
  Serial.print("  Voltaje: ");
  Serial.println(v);
}
