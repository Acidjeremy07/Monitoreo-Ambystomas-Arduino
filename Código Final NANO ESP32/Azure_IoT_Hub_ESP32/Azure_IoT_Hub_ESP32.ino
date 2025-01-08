#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <mqtt_client.h>
#include <ArduinoJson.h>
#include <time.h>

// Librerías de Azure
#include <az_core.h>
#include <az_iot.h>
#include <az_iot_hub_client.h>
#include <az_iot_provisioning_client.h>
#include <az_span.h>
#include <az_json.h>
#include <azure_ca.h>

// Configuración IoT
#include "iot_configs.h"
#include "AzIoTSasToken.h"
#include "SerialLogger.h"

// Definiciones y configuraciones
#define AZURE_SDK_CLIENT_USER_AGENT "c%2F" AZ_SDK_VERSION_STRING "(ard;esp32)"
#define sizeofarray(a) (sizeof(a) / sizeof(a[0]))
#define NTP_SERVERS "pool.ntp.org", "time.nist.gov"
#define MQTT_QOS1 1
#define DO_NOT_RETAIN_MSG 0
#define SAS_TOKEN_DURATION_IN_MINUTES 60
#define UNIX_TIME_NOV_13_2017 1510592825
#define PST_TIME_ZONE -8
#define PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF 1
#define GMT_OFFSET_SECS (PST_TIME_ZONE * 3600)
#define GMT_OFFSET_SECS_DST ((PST_TIME_ZONE + PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF) * 3600)
#define TELEMETRY_FREQUENCY_MILLISECS 600000 // 10 minutos

// Configuración de WiFi y Azure IoT
static const char* ssid = IOT_CONFIG_WIFI_SSID;
static const char* password = IOT_CONFIG_WIFI_PASSWORD;
static const char* host = IOT_CONFIG_IOTHUB_FQDN;
static const char* mqtt_broker_uri = "mqtts://" IOT_CONFIG_IOTHUB_FQDN;
static const char* device_id = IOT_CONFIG_DEVICE_ID;
static const int mqtt_port = AZ_IOT_DEFAULT_MQTT_CONNECT_PORT;

// Manejo de MQTT y Azure IoT
static esp_mqtt_client_handle_t mqtt_client;
static az_iot_hub_client client;
static char mqtt_client_id[128];
static char mqtt_username[128];
static char mqtt_password[200];
static uint8_t sas_signature_buffer[256];
static char telemetry_topic[128];
static String telemetry_payload = "{}";

// Buffer para datos entrantes
#define INCOMING_DATA_BUFFER_SIZE 2048
static char incoming_data[INCOMING_DATA_BUFFER_SIZE];

// Servidor WiFi
WiFiServer wifiServer(8080); // Puerto del servidor

// Variables de tiempo
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;

// Variable para almacenar el cliente conectado (Uno R4 WiFi)
WiFiClient activeClient;
bool clientConnected = false;

// SAS Token
#ifndef IOT_CONFIG_USE_X509_CERT
static AzIoTSasToken sasToken(
    &client,
    AZ_SPAN_FROM_STR(IOT_CONFIG_DEVICE_KEY),
    AZ_SPAN_FROM_BUFFER(sas_signature_buffer),
    AZ_SPAN_FROM_BUFFER(mqtt_password));
#endif

// Función para reenviar JSON al Arduino Uno R4 WiFi
static void sendJSONToUno(const String& json) {
  if (clientConnected && activeClient.connected()) {
    Serial.println("Enviando JSON de respuesta al Arduino Uno R4 WiFi:");
    Serial.println(json);
    activeClient.println(json);
  } else {
    Serial.println("No hay cliente conectado para enviar JSON de vuelta.");
  }
}

static void connectToWiFi()
{
  Serial.println("Conectando a WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado. IP: " + WiFi.localIP().toString());
}

static void initializeTime()
{
  Serial.println("Configurando tiempo via NTP");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  time_t now = time(nullptr);
  while (now < UNIX_TIME_NOV_13_2017) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  Serial.println("Tiempo inicializado!");
}

static void generateTelemetryPayload(String jsonData)
{
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, jsonData);

  if (error) {
    Serial.print("Error al parsear JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Obtener timestamp actual
  time_t now = time(NULL);
  struct tm* timeinfo = gmtime(&now);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", timeinfo);

  doc["timestamp"] = timestamp;

  String modifiedJson;
  serializeJson(doc, modifiedJson);
  telemetry_payload = modifiedJson;
}

static void sendTelemetryToAzure()
{
  Serial.println("Enviando telemetría a Azure IoT Hub...");

  if (az_result_failed(az_iot_hub_client_telemetry_get_publish_topic(
          &client, NULL, telemetry_topic, sizeof(telemetry_topic), NULL))) {
    Serial.println("Error al obtener tópico de telemetría");
    return;
  }

  int msg_id = esp_mqtt_client_publish(
      mqtt_client,
      telemetry_topic,
      telemetry_payload.c_str(),
      telemetry_payload.length(),
      MQTT_QOS1,
      DO_NOT_RETAIN_MSG);

  if (msg_id == 0) {
    Serial.println("Error al publicar mensaje");
  } else {
    Serial.println("Mensaje publicado exitosamente a Azure IoT Hub");
  }
}

// Manejo de eventos MQTT
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  (void)handler_args;
  (void)base;
  (void)event_id;

  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

  switch (event->event_id) {
    case MQTT_EVENT_ERROR:
      Serial.println("MQTT_EVENT_ERROR");
      break;
    case MQTT_EVENT_CONNECTED:
      Serial.println("MQTT_EVENT_CONNECTED");
      {
        int r = esp_mqtt_client_subscribe(mqtt_client, AZ_IOT_HUB_CLIENT_C2D_SUBSCRIBE_TOPIC, 1);
        if (r == -1) {
          Serial.println("No se pudo suscribir a mensajes C2D.");
        } else {
          Serial.println("Suscrito a mensajes C2D; ID: " + String(r));
        }
      }
      break;
    case MQTT_EVENT_DISCONNECTED:
      Serial.println("MQTT_EVENT_DISCONNECTED");
      break;
    case MQTT_EVENT_SUBSCRIBED:
      Serial.println("MQTT_EVENT_SUBSCRIBED");
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      Serial.println("MQTT_EVENT_UNSUBSCRIBED");
      break;
    case MQTT_EVENT_PUBLISHED:
      Serial.println("MQTT_EVENT_PUBLISHED");
      break;
    case MQTT_EVENT_DATA: {
      Serial.println("MQTT_EVENT_DATA (Mensaje C2D recibido de Azure)");
      // Extraer el mensaje
      String c2dMessage;
      c2dMessage.reserve(event->data_len);
      for (int i = 0; i < event->data_len; i++) {
        c2dMessage += event->data[i];
      }
      Serial.println("Mensaje C2D: " + c2dMessage);

      // Intentar parsear el mensaje C2D como JSON y reenviarlo al Arduino Uno R4 WiFi
      if (clientConnected && activeClient.connected()) {
        Serial.println("Reenviando mensaje C2D al Arduino Uno R4 WiFi...");
        sendJSONToUno(c2dMessage);
      } else {
        Serial.println("No hay cliente UNO R4 WIFI conectado para reenviar C2D.");
      }
    } break;
    case MQTT_EVENT_BEFORE_CONNECT:
      Serial.println("MQTT_EVENT_BEFORE_CONNECT");
      break;
    default:
      Serial.println("MQTT_EVENT_UNKNOWN");
      break;
  }
}
#else
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  // Para versiones anteriores de ESP Arduino
  return ESP_OK;
}
#endif

static void initializeIoTHubClient()
{
  az_iot_hub_client_options options = az_iot_hub_client_options_default();
  options.user_agent = AZ_SPAN_FROM_STR(AZURE_SDK_CLIENT_USER_AGENT);

  if (az_result_failed(az_iot_hub_client_init(
          &client,
          az_span_create((uint8_t*)host, strlen(host)),
          az_span_create((uint8_t*)device_id, strlen(device_id)),
          &options))) {
    Serial.println("Error al inicializar el cliente Azure IoT Hub");
    return;
  }

  size_t client_id_length;
  if (az_result_failed(az_iot_hub_client_get_client_id(
          &client, mqtt_client_id, sizeof(mqtt_client_id) - 1, &client_id_length))) {
    Serial.println("Error al obtener client ID");
    return;
  }

  if (az_result_failed(az_iot_hub_client_get_user_name(
          &client, mqtt_username, sizeofarray(mqtt_username), NULL))) {
    Serial.println("Error al obtener username MQTT");
    return;
  }

  Serial.println("Client ID: " + String(mqtt_client_id));
  Serial.println("Username: " + String(mqtt_username));
}

static int initializeMqttClient()
{
#ifndef IOT_CONFIG_USE_X509_CERT
  if (sasToken.Generate(SAS_TOKEN_DURATION_IN_MINUTES) != 0) {
    Serial.println("Error al generar SAS token");
    return 1;
  }
#endif

  esp_mqtt_client_config_t mqtt_config;
  memset(&mqtt_config, 0, sizeof(mqtt_config));

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  mqtt_config.broker.address.uri = mqtt_broker_uri;
  mqtt_config.broker.address.port = mqtt_port;
  mqtt_config.credentials.client_id = mqtt_client_id;
  mqtt_config.credentials.username = mqtt_username;

#ifdef IOT_CONFIG_USE_X509_CERT
  Serial.println("Autenticación X509");
  mqtt_config.credentials.authentication.certificate = IOT_CONFIG_DEVICE_CERT;
  mqtt_config.credentials.authentication.certificate_len = (size_t)strlen(IOT_CONFIG_DEVICE_CERT);
  mqtt_config.credentials.authentication.key = IOT_CONFIG_DEVICE_CERT_PRIVATE_KEY;
  mqtt_config.credentials.authentication.key_len = (size_t)strlen(IOT_CONFIG_DEVICE_CERT_PRIVATE_KEY);
#else
  mqtt_config.credentials.authentication.password = (const char*)az_span_ptr(sasToken.Get());
#endif

  mqtt_config.session.keepalive = 30;
  mqtt_config.session.disable_clean_session = 0;
  mqtt_config.network.disable_auto_reconnect = false;
  mqtt_config.broker.verification.certificate = (const char*)ca_pem;
#else
  mqtt_config.uri = mqtt_broker_uri;
  mqtt_config.port = mqtt_port;
  mqtt_config.client_id = mqtt_client_id;
  mqtt_config.username = mqtt_username;

#ifdef IOT_CONFIG_USE_X509_CERT
  Serial.println("Autenticación X509");
  mqtt_config.client_cert_pem = IOT_CONFIG_DEVICE_CERT;
  mqtt_config.client_key_pem = IOT_CONFIG_DEVICE_CERT_PRIVATE_KEY;
#else
  mqtt_config.password = (const char*)az_span_ptr(sasToken.Get());
#endif

  mqtt_config.keepalive = 30;
  mqtt_config.disable_clean_session = 0;
  mqtt_config.disable_auto_reconnect = false;
  mqtt_config.event_handle = mqtt_event_handler;
  mqtt_config.user_context = NULL;
  mqtt_config.cert_pem = (const char*)ca_pem;
#endif

  mqtt_client = esp_mqtt_client_init(&mqtt_config);

  if (mqtt_client == NULL) {
    Serial.println("Error creando cliente MQTT");
    return 1;
  }

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
#endif

  esp_err_t start_result = esp_mqtt_client_start(mqtt_client);

  if (start_result != ESP_OK) {
    Serial.println("No se pudo iniciar cliente MQTT. Error: " + String(start_result));
    return 1;
  } else {
    Serial.println("Cliente MQTT iniciado");
    return 0;
  }
}

static uint32_t getEpochTimeInSecs() { return (uint32_t)time(NULL); }

static void establishConnection()
{
  connectToWiFi();
  initializeTime();
  initializeIoTHubClient();
  (void)initializeMqttClient();
  wifiServer.begin();
  Serial.println("Servidor WiFi iniciado en puerto 8080");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando sistema...");
  establishConnection();
}

void loop()
{
  // Aceptar nuevas conexiones
  if (!clientConnected) {
    WiFiClient newClient = wifiServer.available();
    if (newClient) {
      Serial.println("Cliente conectado desde: " + newClient.remoteIP().toString());
      activeClient = newClient;
      clientConnected = true;
    }
  }

  if (clientConnected && activeClient.connected()) {
    String receivedJson = "";
    unsigned long timeout = millis() + 5000;
    // Leer datos del cliente
    while (activeClient.connected() && millis() < timeout) {
      while (activeClient.available()) {
        char c = activeClient.read();
        receivedJson += c;
        timeout = millis() + 5000; 
      }
    }

    if (receivedJson.length() > 0) {
      receivedJson.trim();
      Serial.println("JSON recibido desde Arduino Uno R4 WiFi:");
      Serial.println(receivedJson);

      // Generar payload con timestamp y enviar a Azure
      generateTelemetryPayload(receivedJson);
      sendTelemetryToAzure();

      // Enviar JSON de respuesta al UNO R4 WiFi
      StaticJsonDocument<200> respDoc;
      respDoc["status"] = "Datos recibidos y enviados a Azure";
      String responseJson;
      serializeJson(respDoc, responseJson);

      sendJSONToUno(responseJson);

      // Cerrar la conexión para este ejemplo (si deseas mantenerla abierta, omite el stop)
      activeClient.stop();
      clientConnected = false;
      Serial.println("Cliente desconectado.");
    }
  } else {
    // Si no hay cliente o cliente no conectado, hacer otras tareas
  }

#ifndef IOT_CONFIG_USE_X509_CERT
  // Verificar expiración del SAS token
  if (sasToken.IsExpired()) {
    Serial.println("SAS token expirado; reconectando con uno nuevo.");
    esp_mqtt_client_destroy(mqtt_client);
    initializeMqttClient();
  }
#endif

  // La biblioteca MQTT maneja eventos por callbacks, no se requiere poll
  delay(10);
}
