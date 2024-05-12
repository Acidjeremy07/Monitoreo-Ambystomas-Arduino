// Para programar el ESP01 se recomienda que se ponga el gpio0 en GND ya que de esta forma entra en el modo de
// programación, además de darle un reset al ESP01, esto se hace de igual manera poniendo a GND el pin de RST
// de manera rápida, una vez que se termine de cargar el programa igual por recomendación que se haga un reset
// después de la programción. Así mismo después de programar el ESP01 el gpio0 se recomienda que no este conectado 
// a nada 
// Para este código el RX y TX del esp01 se deben conectar al RX y TX 0 de la placa de arduino mega, en caso de 
// usar arduino uno simplemente en los pines RX y TX esto debido a que arduino mega tiene más pines para la 
// comunicación serial 
// Dentro de este programa se realiza la conexión a internet para la comunicación entre Flask y el arduino mega
// para el arduino el ESP01 obtiene un JSON y lo convierte en texto plano así como también recibe texto plano y
// lo convierte en JSON para un mejor uso del ESP01 y menor complejidad en el código de Flask

// Mejoras: Que todo se maneje a JSON y quitar los Debuggers
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "kamehouse";
const char* password = "Oliver-jeremy-051177";
const char* serverUrlCommand = "http://192.168.68.109:5000/command";
const char* serverUrlData = "http://192.168.68.109:5000/data";

WiFiClient client;

void setup() {
  Serial.begin(115200);
  connectToWiFi();
}

void loop() {
  // Recepción de comandos desde Flask y envío al Arduino Mega
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(client, serverUrlCommand);
    int httpCode = http.GET();

if (httpCode == 200) {
  String payload = http.getString();
  Serial.println("Received from Flask: " + payload); // Debug
  // Suponiendo que la carga útil sea una cadena JSON simple
  int startPos = payload.indexOf(":\"") + 2;
  int endPos = payload.indexOf("\"}", startPos);
  String command = payload.substring(startPos, endPos);
  Serial.println("Sending to Arduino: " + command); // Debug
  Serial1.println(command); // Send command to Arduino
}

    http.end();
  }

  // Envío de datos de pH al servidor Flask
  if (Serial1.available()) {
    String pHData = Serial1.readStringUntil('\n');  // Lee los datos de pH en formato JSON
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(client, serverUrlData);
      http.addHeader("Content-Type", "application/json");
      http.POST(pHData);
      http.end();
    }
  }


  delay(5000); // Espera 5 segundos entre cada operación
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}
