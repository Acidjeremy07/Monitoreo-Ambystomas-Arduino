#include <WiFi.h>
#include <ArduinoJson.h>

const char* ssid = "INFINITUM0FFB";       // Reemplaza con tu SSID
const char* password = "YbJy7NeRSJ";      // Reemplaza con tu contraseña

const char* host = "192.168.1.71";         // IP del Arduino Nano ESP32 (servidor)
WiFiClient client;

void setup() {
  Serial.begin(115200);

  // Conectar a la red Wi-Fi
  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    Serial.println("Conectando a WiFi...");
    delay(1000);
  }
  Serial.println("Conectado a Wi-Fi");
  Serial.print("IP del cliente: ");
  Serial.println(WiFi.localIP());  // Mostrar la IP del Arduino Uno R4 Wi-Fi
}

void loop() {
  if (client.connect(host, 80)) {  // Conectar al servidor (Arduino Nano ESP32)
    Serial.println("Conectado al servidor!");

    // Enviar la solicitud HTTP GET para recibir el JSON
    client.print("GET / HTTP/1.1\r\n");
    client.print("Host: ");
    client.print(host);
    client.print("\r\n");
    client.print("Connection: close\r\n");
    client.print("\r\n");

    // Esperar a recibir la respuesta
    String response = "";
    while (client.available()) {
      char c = client.read();
      response += c;
    }

    // Mostrar la respuesta en el monitor serial
    Serial.println("Respuesta recibida:");
    Serial.println(response);

    // Verificar que la respuesta no esté vacía antes de procesarla
    if (response.length() > 0) {
      // Procesar el JSON recibido
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, response);

      if (error) {
        Serial.print(F("Error al deserializar JSON: "));
        Serial.println(error.f_str());
      } else {
        // Mostrar el contenido del JSON recibido
        const char* status = doc["status"];
        const char* message = doc["message"];

        Serial.print("Estado: ");
        Serial.println(status);
        Serial.print("Mensaje: ");
        Serial.println(message);
      }
    } else {
      Serial.println("Error: La respuesta está vacía.");
    }

    client.stop();  // Cerrar la conexión con el servidor
  } else {
    Serial.println("Conexión fallida");
  }

  delay(10000);  // Esperar 10 segundos antes de hacer otro intento
}
