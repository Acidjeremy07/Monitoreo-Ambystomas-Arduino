// Definir el pin de entrada analógico
int sensorPin = A3;

// Valores de calibración: estos deben ajustarse según tus resultados reales
int valorAguaClara = 200;  // Valor analógico en agua clara (0 ppm)
int valor500ppm = 700;     // Valor analógico en la solución de calibración de 500 ppm

void setup() {
    Serial.begin(9600);  // Inicializar la comunicación serial
}

void loop() {
    int valorSensor = analogRead(sensorPin);  // Leer el valor analógico del sensor
    float tss = calibrarTSS(valorSensor);     // Convertir el valor a ppm usando la función de calibración

    // Mostrar los valores en el monitor serial
    Serial.print("Valor del Sensor: ");
    Serial.print(valorSensor);
    Serial.print(" - TSS (ppm): ");
    Serial.println(tss);

    delay(1000);  // Esperar un segundo antes de la siguiente lectura
}

// Función para calcular TSS (en ppm) basado en los valores de calibración
float calibrarTSS(int valor) {
    // Cálculo de TSS usando una ecuación lineal entre los valores de 0 ppm y 500 ppm
    float tss = (float)(valor - valorAguaClara) * (500.0 / (valor500ppm - valorAguaClara));
    
    // Asegurarse de que no haya valores negativos
    if (tss < 0) tss = 0;

    return tss;
}
