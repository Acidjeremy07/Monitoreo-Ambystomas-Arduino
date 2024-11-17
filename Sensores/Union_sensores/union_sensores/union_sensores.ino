//LIBRERIAS DE SENSOR TDS
#include <EEPROM.h>
#include "GravityTDS.h"
//LIBRERIAS DE SENSOR DE TEMPERATURA 
#include <OneWire.h>                
#include <DallasTemperature.h>
/***********Calibracion TDS ***************
     Calibration CMD:
     enter -> ingrese enter para inciar la calbración
     cal:tds value -> Calibrar con el valor de TDS conocido (25°C). Ejemplo: cal:707
     exit -> Guardar los parámetros y salir del modo de calibración
 ****************************************************/
#define TdsSensorPin A1
GravityTDS gravityTds;
float temperature = 25,tdsValue = 0;

// VARIABLES DE SENSOR PH
const int PHPin = A0;  //Sensor conectado en PIN A0

//𝒚=−𝟓.𝟕𝒙+𝟐𝟏.𝟒
//y = -9.53x + 42.59  nuestro valor

const float b = 42.59;  //Constante de la ecuación (Y = mx + b)
const float m = -9.53;  //Pendiente de la recta (Y = mx + b)

//VARIABLES DE SENSOR DE TEMPERATURA
OneWire ourWire(2);                //Se establece el pin 2  como bus OneWire
 
DallasTemperature sensors(&ourWire); //Se declara una variable u objeto para nuestro sensor

//ELECCIÓN DE SENSORES
const uint8_t sensors_buff = 5;
char selected_sensors[sensors_buff] = {0,0,0,0,0};
String opc = "";
int cant_medi = "";
int time = "";



void setup() {
  delay(1000);
  Serial.begin(9600);
  sensors.begin(); 

  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);          //Voltaje de referencia en el ADC, por defecto 5.0V en Arduino UNO
  gravityTds.setAdcRange(1024);     //1024 para 10bit ADC;4096 para 12bit ADC
  gravityTds.begin();              
}

void loop() {
  //CALIBRACIÓN DE LOS SENSORES
  if(opc.equals("CAL"))
  {
    for(int i = 0; i < sensors_buff; i++)
    {
      if(selected_sensors[i] == '1')
      {
        if(i==2)
        {

        }
        if(i==3)
        {

        }
        if(i==4)
        {

        }
        if(i==5)
        {

        }
      }
    }
  }

  //MEDICIONES DE LOS SENSORES
  if(opc.equals("INI"))
  {
    for(int i = 0; i < sensors_buff; i++)
    {
      if(selected_sensor[i] == '1')
      {
        if(i==0)
        {
          PHpromedio();  //función que calcula el ph promedio de 10 muestras
          delay(1000);
        }
        if(i==1)
        {
          //temperature = readTemperature();          // Añade tu sensor de temperatura para su lectura analogica.
          gravityTds.setTemperature(temperature);     // Establece la temperatura y ejecuta la compensación de temperatura.
          gravityTds.update();                        // Muestrea y calcula.
          tdsValue = gravityTds.getTdsValue();        // Luego, obtén el valor.
          Serial.print(tdsValue,0);
          Serial.println("ppm");
          delay(1000);
        }
        if(i==2)
        {
          sensors.requestTemperatures();   //Se envía el comando para leer la temperatura
          float temp= sensors.getTempCByIndex(0); //Se obtiene la temperatura en ºC

          Serial.print("Temperatura= ");
          Serial.print(temp);
          Serial.println(" C");
          delay(100);            
        }
        if(i==3)
        {
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
          for(int i=0; i<rep; i++){

          sensorValue = analogRead(A0);// read the input on analog pin 0:
          voltage = sensorValue * (5.0 / 1024.0); // Convert the analog reading (which goes from 0 – 1023) to a voltage (0 – 5V):
          tempA25 =voltCal-varTemp;
          K = -865.68*tempA25;
          ntu =(-865.68*voltage)-K;

          if(ntu<0){
            ntu=0;  
          }
            
              suma=suma +ntu;
          }
          ntu = suma/rep;

          Serial.println("CRUDO"); // print out the value you read:
          Serial.println(sensorValue); // print out the value you read:
          Serial.println("VOLTAJE A 5V"); // print out the value you read:
          Serial.println(voltage); // print out the value you read:
          Serial.println("NTU"); // print out the value you read:
          Serial.println(round(ntu)); // print out the value you read:
          delay(1000);
        }
        if(i==4)
        {

        }
      }
    }  
  }
}


void PHpromedio() {
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
}


void impresion(float ph, float v) {

  Serial.print("Ph: ");
  Serial.print(ph);
  Serial.print("  Voltaje: ");
  Serial.println(v);
}