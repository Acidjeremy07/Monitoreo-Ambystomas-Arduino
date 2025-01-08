#define RX2 17  // Pin RX2 en Arduino Mega
#define TX2 16  // Pin TX2 en Arduino Mega
String lecturaSensorOD = ""; // Variable para almacenar la lectura del sensor de OD

void setup() {
    // Inicializar la comunicación serial para el monitor
    Serial.begin(9600);
    
    // Inicializar la comunicación serial para el sensor OD en Serial2
    Serial2.begin(9600);  // Ajusta la velocidad de baudios si es necesario para tu sensor de OD de Atlas Scientific

    // Mensaje inicial
    Serial.println("Leyendo datos del sensor de OD de Atlas Scientific...");
}

void loop() {
    // Leer los datos enviados por el sensor de OD en Serial2
    if (Serial2.available()) {
        char caracter = Serial2.read();
        
        if (caracter == '\r') {  // Si recibe el carácter de fin de línea
            Serial.print("Lectura del Sensor OD: ");
            Serial.println(lecturaSensorOD);
            lecturaSensorOD = "";  // Limpiar el buffer de lectura para la siguiente lectura
        } else {
            lecturaSensorOD += caracter;  // Agregar caracteres a la lectura hasta el fin de línea
        }
    }

    delay(500);  // Espera 500 ms entre lecturas para reducir el flujo de datos
}
