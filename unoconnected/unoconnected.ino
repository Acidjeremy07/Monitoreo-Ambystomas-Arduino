#include <WiFi.h>
#include <WiFiSSLClient.h>
#include <PubSubClient.h>
#include <Crypto.h>
#include <SHA256.h>
#include <Base64.h>

// Configuración WiFi
const char* ssid = "TU_SSID";         // Reemplaza con el nombre de tu red WiFi
const char* password = "TU_PASSWORD"; // Reemplaza con la contraseña de tu red WiFi

// Configuración de Azure IoT
const char* hostName = "SistemaMonitoreo.azure-devices.net";
const char* deviceId = "DEV-AXOLOTL-01";
const char* sharedAccessKey = "Jf4YNyK2RUBdm5o7F7ewJ79PAw3nOaAjl4S8XJ226lU=";

// Variables de conexión MQTT
const int mqttPort = 8883;
WiFiSSLClient net;
PubSubClient client(net);

// Función para codificar en base64
String encodeBase64(const uint8_t* input, size_t length) {
  char encodedData[Base64.encodedLength(length) + 1];
  Base64.encode(encodedData, (char*)input, length);
  return String(encodedData);
}

// Función para generar el SAS Token
String createSASToken(String resourceUri, String key, int expiryTime) {
  String stringToSign = resourceUri + "\n" + String(expiryTime);

  // Decodificar clave base64
  size_t keyLength = Base64.decodedLength((char*)key.c_str(), key.length());
  uint8_t decodedKey[keyLength];
  Base64.decode((char*)decodedKey, (char*)key.c_str(), key.length());

  // Generar HMAC-SHA256
  uint8_t hmacResult[32];
  SHA256 sha256;
  sha256.resetHMAC(decodedKey, keyLength);
  sha256.update((const uint8_t*)stringToSign.c_str(), stringToSign.length());
  sha256.finalizeHMAC(decodedKey, keyLength, hmacResult, sizeof(hmacResult));

  // Codificar el resultado HMAC en base64
  String signature = encodeBase64(hmacResult, sizeof(hmacResult));

  // Crear el SAS Token
  String sasToken = "SharedAccessSignature sr=" + resourceUri +
                    "&sig=" + signature +
                    "&se=" + String(expiryTime);
  return sasToken;
}

// Conexión a Azure IoT
void connectToAzure() {
  String resourceUri = String(hostName) + "/devices/" + deviceId;
  int expiryTime = (int)(millis() / 1000) + 3600; // Expira en 1 hora
  String sasToken = createSASToken(resourceUri, sharedAccessKey, expiryTime);

  client.setServer(hostName, mqttPort);

  while (!client.connected()) {
    Serial.println("Conectando a Azure IoT...");
    String mqttUsername = String(hostName) + "/" + deviceId + "/?api-version=2020-09-30";
    if (client.connect(deviceId, mqttUsername.c_str(), sasToken.c_str())) {
      Serial.println("Conectado a Azure IoT!");
    } else {
      Serial.print("Error de conexión. Código: ");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Conectar a WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a WiFi!");

  // Configurar conexión SSL
  net.setCACert("-----BEGIN CERTIFICATE-----\n"
                "MIIC... (Certificado raíz de Azure IoT)\n"
                "-----END CERTIFICATE-----");

  connectToAzure();
}

void loop() {
  if (!client.connected()) {
    connectToAzure();
  }
  client.loop();

  // Publicar mensaje
  String topic = "devices/" + String(deviceId) + "/messages/events/";
  String message = "{\"temperature\": 22.5, \"humidity\": 60}";
  if (client.publish(topic.c_str(), message.c_str())) {
    Serial.println("Mensaje publicado: " + message);
  } else {
    Serial.println("Error publicando el mensaje");
  }

  delay(5000);
}
