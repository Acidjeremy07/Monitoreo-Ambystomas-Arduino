// Libreria para conexión con ESP01
#include <SoftwareSerial.h>
#define DEBUG true
//LIBRERIAS DE SENSOR TDS
#include <EEPROM.h>
#include "GravityTDS.h"
//LIBRERIAS DE SENSOR DE TEMPERATURA 
#include <OneWire.h>                
#include <DallasTemperature.h>

// Configuración de pines
#define TdsSensorPin A1   //solidos disueltos TDS meter v1.0 (color negro)
#define PHPin A0          //ph
#define ODin A2           //oxigeno disuelto(atlas scientific amarillo)
#define TempSensorPin 22  //temperatura (sonda plateada)
#define TurbidityPin A3   // Pin de turbidez(resina)(TSS)

//Creación de instancia del objeto para el sensor TDS
GravityTDS gravityTds;
//Constante de calibración de TDS
float temperature = 25,tdsValue = 0;

// VARIABLES DE SENSOR PH
//
//𝒚=−𝟓.𝟕𝒙+𝟐𝟏.𝟒
//y = -9.53x + 42.59  nuestro valor

const float b = 42.59;  //Constante de la ecuación (Y = mx + b)
const float m = -9.53;  //Pendiente de la recta (Y = mx + b)

//VARIABLES DE SENSOR DE TEMPERATURA
OneWire ourWire(2);                //Se establece el pin 2  como bus OneWire
DallasTemperature sensors(&ourWire); //Se declara una variable u objeto para nuestro sensor

//VARIABLES DE SENSOR DE OD
float calibracion = 0;
int ini = 0;
int connectionId;
float ntu;
//Bandera de inicialización
int initialized = 0;

//Modulo WiFi
SoftwareSerial esp8266(19,18);

void setup() {
  delay(1000);
  Serial.begin(115200);
  esp8266.begin(115200);
  sensors.begin(); 

  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);          //Voltaje de referencia en el ADC, por defecto 5.0V en Arduino UNO
  gravityTds.setAdcRange(1024);     //1024 para 10bit ADC;4096 para 12bit ADC
  gravityTds.begin();     

  sendCommand("AT+RST\r\n",2000,DEBUG); // reset module
  sendCommand("AT+CWMODE=1\r\n",1000,DEBUG); // configure as access point
  sendCommand("AT+CWJAP=\"kamehouse\",\"051177\"\r\n",3000,DEBUG);
  delay(10000);
  sendCommand("AT+CIFSR\r\n",1000,DEBUG); // get ip address
  sendCommand("AT+CIPMUX=1\r\n",1000,DEBUG); // configure for multiple connections
  sendCommand("AT+CIPSERVER=1,80\r\n",1000,DEBUG); // turn on server on port 80
  
  Serial.println("Server Ready");         
}

void loop() 
{
  if(esp8266.available())
  {
    if(esp8266.find("+IPD,"))
    {
      delay(1000);
      int connectionId = esp8266.read()-48;

      //Iniciar la construcción del contenido HTML
      String content = "<¡DOCTYPE html><html><head><title>Estación de Monitoreo</title></head><body>";
      content += "<h1>Datos de sensores</h1>";
      //SENSOR DE PH
      float ph = PHpromedio();  //función que calcula el ph promedio de 10 muestras
      content += "<p>PH: " + String(ph) + "</p>";
      delay(1000);

      //SENSOR DE TOTAL DE SOLIDOS DISUELTOS
      gravityTds.setTemperature(temperature);     // Establece la temperatura y ejecuta la compensación de temperatura.
      gravityTds.update();                        // Muestrea y calcula.
      tdsValue = gravityTds.getTdsValue();        // Luego, obtén el valor.
      content += "<p>TDS: " + String(tdsValue) + " ppm</p>";
      delay(1000);
        
      //SENSOR DE TEMPERATURA
      sensors.requestTemperatures();   //Se envía el comando para leer la temperatura
      float temp= sensors.getTempCByIndex(0); //Se obtiene la temperatura en ºC

      content += "Temperatura: " + String(temp) + " C</p>";
      delay(1000);            
          
      //SENSOR DE TOTAL DE SOLIDOS SUSPENDIDOS
      float tempCal = 25;  // temperatura solucion de calibracion
      float voltCal = 1.10; // voltaje de solucion de calibracion
      float voltage = 0;
      float varTemp = 0; 
      float tempA25 = 0;
      float K = 0;
      float ntu = 0;
      int sensorValue = 0;

      float suma=0;
      int rep = 30;
      for(int i=0; i<rep; i++)
      {
        sensorValue = analogRead(A3);// read the input on analog pin 0:
        voltage = sensorValue * (5.0 / 1024.0); // Convert the analog reading (which goes from 0 – 1023) to a voltage (0 – 5V):
        tempA25 =voltCal-varTemp;
        K = -865.68*tempA25;
        ntu =(-865.68*voltage)-K;

        if(ntu<0)
        {
          ntu=0;  
        }
        suma=suma +ntu;
      }
      ntu = suma/rep;

      content += "<p>TSS: " + String(ntu) + "ntu</p>";
      delay(1000);
            
      //SENSOR DE OXIGENO DISUELTO
      if(ini == 0) //10 segundos de calibración 
      {
        calibracion = voltajepromedio();
        ini = ini + 1;
      }
      float porcentaje = oxigenodisuelto(calibracion);
      content += "<p>% de oxigeno disuelto: " + String(porcentaje) + "%</p>";
      delay(1000);
    
       // Cerrar conexión
        String closeCommand = "AT+CIPCLOSE="; 
        closeCommand += connectionId;
        closeCommand += "\r\n";
        sendCommand(closeCommand, 1000, DEBUG);
    }  
  }
}

float PHpromedio() {
  long muestras;       //Variable para las 10 muestras
  float muestrasprom;  //Variable para calcular el promedio de las muestras

  muestras = 0;
  muestrasprom = 0;


  //10 muestras, una cada 10ms
  for (int x = 0; x < 10; x++) {
    muestras += analogRead(PHPin);
    delay(10);
  }

  muestrasprom = muestras / 10.0;

  float phVoltaje = muestrasprom * (5.0 / 1024.0);  //se calcula el voltaje

  float pHValor = phVoltaje * m + b;  //Aplicando la ecuación se calcula el PH

  impresion(pHValor, phVoltaje);
  return pHValor;
}


void impresion(float ph, float v) {

  Serial.print("Ph: ");
  Serial.print(ph);
  Serial.print("  Voltaje: ");
  Serial.println(v);
}

void impresion(float v) 
{
  Serial.print("Voltaje: ");
  Serial.println(v);
  Serial.print("mv");
}

float oxigenodisuelto(float vc)
{
  long muestras;       //Variable para las 10 muestras
  float muestrasprom;  //Variable para calcular el promedio de las muestras
  float porcentaje;

  muestras = 0;
  muestrasprom = 0;


  //10 muestras, una cada 10ms
  for (int x = 0; x < 1000; x++) {
    muestras += analogRead(ODin);
    delay(10);
  }

  muestrasprom = muestras / 1000.0;

  float Voltaje = (muestrasprom * (5.0 / 1024.0) * 1000);
  porcentaje = (Voltaje / vc) * 100;
  Serial.print("Porcentaje de DO: ");
  Serial.print("porcentaje");
}

float voltajepromedio() {
  long muestras;       //Variable para las 10 muestras
  float muestrasprom;  //Variable para calcular el promedio de las muestras

  muestras = 0;
  muestrasprom = 0;


  //10 muestras, una cada 10ms
  for (int x = 0; x < 1000; x++) {
    muestras += analogRead(ODin);
    delay(10);
  }

  muestrasprom = muestras / 1000.0;

  float Voltaje = (muestrasprom * (5.0 / 1024.0) * 1000);  //se calcula el voltaje

  impresion(Voltaje);
  return Voltaje;
}

String sendData(String command, const int timeout, boolean debug)
{
    String response = "";
    
    int dataSize = command.length();
    char data[dataSize];
    command.toCharArray(data,dataSize);
           
    esp8266.write(data,dataSize); // send the read character to the esp8266
    if(debug)
    {
      Serial.println("\r\n====== HTTP Response From Arduino ======");
      Serial.write(data,dataSize);
      Serial.println("\r\n========================================");
    }
    
    long int time = millis();
    
    while( (time+timeout) > millis())
    {
      while(esp8266.available())
      {
        
        // The esp has data so display its output to the serial window 
        char c = esp8266.read(); // read the next character.
        response+=c;
      }  
    }
    
    if(debug)
    {
      Serial.print(response);
    }
    
    return response;
}
 
/*
* Name: sendHTTPResponse
* Description: Function that sends HTTP 200, HTML UTF-8 response
*/
void sendHTTPResponse(int connectionId, String content)
{
     
     // build HTTP response
     String httpResponse;
     String httpHeader;
     // HTTP Header
     httpHeader = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n"; 
     httpHeader += "Content-Length: ";
     httpHeader += content.length();
     httpHeader += "\r\n";
     httpHeader +="Connection: close\r\n\r\n";
     httpResponse = httpHeader + content + " "; // There is a bug in this code: the last character of "content" is not sent, I cheated by adding this extra space
     sendCIPData(connectionId,httpResponse);
}
 
/*
* Name: sendCIPDATA
* Description: sends a CIPSEND=<connectionId>,<data> command
*
*/
void sendCIPData(int connectionId, String data)
{
   String cipSend = "AT+CIPSEND=";
   cipSend += connectionId;
   cipSend += ",";
   cipSend +=data.length();
   cipSend +="\r\n";
   sendCommand(cipSend,1000,DEBUG);
   sendData(data,1000,DEBUG);
}
 
/*
* Name: sendCommand
* Description: Function used to send data to ESP8266.
* Params: command - the data/command to send; timeout - the time to wait for a response; debug - print to Serial window?(true = yes, false = no)
* Returns: The response from the esp8266 (if there is a reponse)
*/
String sendCommand(String command, const int timeout, boolean debug)
{
    String response = "";
           
    esp8266.print(command); // send the read character to the esp8266
    
    long int time = millis();
    
    while( (time+timeout) > millis())
    {
      while(esp8266.available())
      {
        
        // The esp has data so display its output to the serial window 
        char c = esp8266.read(); // read the next character.
        response+=c;
      }  
    }
    
    if(debug)
    {
      Serial.print(response);
    }
    return response;
}