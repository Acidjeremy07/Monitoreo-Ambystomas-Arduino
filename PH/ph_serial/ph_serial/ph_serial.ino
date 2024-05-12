// Dentro de este código se puede realizar la calibración para el sensor de pH por un punto, así como iniciar
// la toma de mediciones de pH así como detener la medición
// Se recibe por medio del ESP01 el comando para definir la acción que tomará el sistema, los comandos entran
// como texto plano y sale de igual manera como texto plano 
const int PHPin = A0;  // Sensor conectado en PIN A0

volatile float b = 38.308;  // Constante de la ecuación (Y = mx + b)
volatile float m = -9.53;  // Pendiente de la recta (Y = mx + b)
bool shouldMeasure = false;  // Flag para comenzar mediciones

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200); // Comunicación con el ESP01
}

void loop(){
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    handleInput(input);
  }
  if (Serial1.available() > 0) {
    String input = Serial1.readStringUntil('\n');
    handleInput(input);  // Manejar entrada desde el ESP01
  }

  if (shouldMeasure) {
    PHpromedio();  // función que calcula el ph promedio de 10 muestras
    delay(1000);  // Medición cada segundo
  }
}

void handleInput(String input) {
  input.trim(); // Elimina espacios en blanco y caracteres de nueva línea
  
  // Vamos a buscar el inicio y el fin del comando dentro del JSON
  int startIdx = input.indexOf("\"command\":\"") + 11; // Busca el inicio del comando
  int endIdx = input.indexOf("\"", startIdx); // Busca el final del comando
  
  // Si no se encuentran las posiciones, entonces no se hace nada
  if (startIdx == -1 || endIdx == -1) {
    return; // No se ha encontrado un comando válido, por lo que salimos de la función
  }

  // Extrae el comando real del JSON
  String command = input.substring(startIdx, endIdx);

  // Ahora sí, compara el comando extraído con las opciones conocidas
  if (command == "calibrate") {
    shouldMeasure = false;  // Detener la medición mientras se calibra
    // Debes asegurarte de obtener el valor numérico después de "calibrate"
    // Por ejemplo, si el comando es "calibrate 7.00", extraemos "7.00" y lo convertimos a float
    int valueStartIdx = input.indexOf(" ", endIdx) + 1; // Encuentra el inicio del valor de calibración
    if (valueStartIdx > 0) { // Verifica si hay un valor
      float calibrationPH = input.substring(valueStartIdx).toFloat();
      calibrate(calibrationPH);
    }
    Serial.println("Comando de calibración recibido."); // Mensaje de depuración
  } else if (command == "start") {
    shouldMeasure = true;  // Comenzar mediciones
    Serial.println("Comando 'start'recibido, comenzando mediciones."); // Mensaje de depuración
  } else if (command == "stop") {
    shouldMeasure = false; // Detener mediciones
  Serial.println("Comando 'stop' recibido, deteniendo mediciones."); // Mensaje de depuración
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
  Serial.print(" Voltaje: ");
  Serial.println(v);

  String json = "{\"ph\":\"" + String(ph, 2) + "\", \"voltaje\":\"" + String(v, 2) + "\"}";
  Serial1.println(json);  // Enviar a ESP01
}