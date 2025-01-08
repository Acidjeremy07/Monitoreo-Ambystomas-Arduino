#include <WiFi.h>
#include <ArduinoJson.h>

const char* ssid = "MERCUSYS_6484";        // Reemplaza con tu SSID
const char* password = "bruno23l";       // Reemplaza con tu contraseña

WiFiServer server(80); // Puerto 80 para el servidor

void setup() {
  Serial.begin(115200);

  // Conectando a la red Wi-Fi
  WiFi.begin(ssid, password);

  // Esperar a que se conecte
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }

  Serial.println("Conectado a Wi-Fi");
  Serial.print("IP del servidor: ");
  Serial.println(WiFi.localIP()); // Muestra la IP local del ESP32

  // Iniciar el servidor
  server.begin();
}

void loop() {
  WiFiClient client = server.available();  // Escuchar por clientes entrantes

  if (client) {
    String request = "";
    bool headersEnded = false;

    // Leer hasta que el cliente termine de enviar datos
    while (client.available()) {
      char c = client.read();

      // Verificar si se ha llegado al final de los encabezados HTTP
      if (!headersEnded) {
        request += c;

        // Detectar el final de los encabezados HTTP (2 saltos de línea)
        if (request.endsWith("\r\n\r\n")) {
          headersEnded = true;
          request = "";  // Limpiar para almacenar solo el cuerpo del mensaje
        }
      } else {
        // Almacenar el cuerpo del mensaje (el JSON)
        request += c;
      }
    }

    // Si hay contenido después de los encabezados HTTP (JSON)
    if (headersEnded && request.length() > 0) {
      Serial.println("Petición recibida:");
      Serial.println(request);  // Mostrar lo que el cliente envía (JSON)

      // Procesar el JSON recibido
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, request);

      if (error) {
        Serial.print(F("Error al deserializar JSON: "));
        Serial.println(error.f_str());
      } else {
        // Mostrar el JSON recibido
        Serial.println("JSON recibido:");
        serializeJsonPretty(doc, Serial);  // Serializar y mostrar el JSON de manera legible
        Serial.println();
      }

      // Responder con un JSON
      StaticJsonDocument<200> response;
      response["status"] = "OK";
      response["message"] = "Conexión establecida correctamente";

      String responseStr;
      serializeJson(response, responseStr);

      // Enviar respuesta HTTP completa con encabezados correctos
      client.print("HTTP/1.1 200 OK\r\n");
      client.print("Content-Type: application/json\r\n");
      client.print("Connection: close\r\n");  // Cerrar la conexión después de la respuesta
      client.print("Content-Length: ");
      client.print(responseStr.length());
      client.print("\r\n\r\n");  // Fin de los encabezados HTTP
      client.print(responseStr); // Enviar el cuerpo del mensaje (JSON)

      // Mostrar el JSON de la respuesta en el monitor serial
      Serial.println("JSON enviado:");
      serializeJsonPretty(response, Serial);
      Serial.println();
    }

    // El servidor sigue esperando más datos, no se cierra la conexión
    delay(200);  // Pequeño retardo para no saturar el servidor
  }
}
