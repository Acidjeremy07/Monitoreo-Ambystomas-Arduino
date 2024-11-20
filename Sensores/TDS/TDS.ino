/**********************************************************************************
 * TITULO: Sensor de TDS
 * AUTOR: Gregory Bustamante
 * DESCRIPCION: Medir la conductividad del agua usando la libreria de gravity.  
 * CANAL YOUTUBE: https://www.youtube.com/c/jadsatv
 * © 2023
 * *******************************************************************************/

 /***********Calibracion TDS ***************
     Calibration CMD:
     enter -> ingrese enter para inciar la calbración
     cal:tds value -> Calibrar con el valor de TDS conocido (25°C). Ejemplo: cal:707
     exit -> Guardar los parámetros y salir del modo de calibración
 ****************************************************/

#include <EEPROM.h>
#include "GravityTDS.h"

#define TdsSensorPin A0
GravityTDS gravityTds;

float temperature = 25,tdsValue = 0;

void setup()
{
    Serial.begin(115200);
    gravityTds.setPin(TdsSensorPin);
    gravityTds.setAref(5.0);          //Voltaje de referencia en el ADC, por defecto 5.0V en Arduino UNO
    gravityTds.setAdcRange(1024);     //1024 para 10bit ADC;4096 para 12bit ADC
    gravityTds.begin();               //inicializamos 
}

void loop()
{
    //temperature = readTemperature();          // Añade tu sensor de temperatura para su lectura analogica.
    gravityTds.setTemperature(temperature);     // Establece la temperatura y ejecuta la compensación de temperatura.
    gravityTds.update();                        // Muestrea y calcula.
    tdsValue = gravityTds.getTdsValue();        // Luego, obtén el valor.
    Serial.print(tdsValue,0);
    Serial.println("ppm");
    delay(1000);
}
