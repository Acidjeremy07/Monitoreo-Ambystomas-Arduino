// Definición de pin para el LED externo
const int ledExterno = 12;

void setup() {
  // Iniciar la comunicación serial a 115200 baudios
  Serial.begin(115200);
  
  // Configurar el pin 13 como salida para el LED interno
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Configurar el pin 12 como salida para el LED externo
  pinMode(ledExterno, OUTPUT);
  
  // Enviar un mensaje de bienvenida al Serial Monitor
  Serial.println("Arduino Mega funcionando correctamente.");
}

void loop() {
  // Contar desde 0 hasta 9
  for (int i = 0; i < 10; i++) {
    // Enviar el número actual al Serial Monitor
    Serial.print("Contando: ");
    Serial.println(i);
    
    // Encender ambos LEDs
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(ledExterno, HIGH);
    // Esperar 500 ms
    delay(500);
    
    // Apagar ambos LEDs
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(ledExterno, LOW);
    // Esperar 500 ms
    delay(500);
  }
}
