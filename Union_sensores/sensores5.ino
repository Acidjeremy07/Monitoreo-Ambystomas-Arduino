#ifdef USE_PULSE_OUT
  #include "do_iso_grav.h"       
  Gravity_DO_Isolated DO = Gravity_DO_Isolated(A0);         
#else
  #include "do_grav.h"
  Gravity_DO DO = Gravity_DO(A0);
#endif

#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include "GravityTDS.h"
#include <TinyGPS.h>

//Constantes necesarias para el sensor de TDS
#define tdsPin A1 //Pin del sensor TDS

//Modulo GPS
TinyGPS gps;
//SoftwareSerial ss(4, 3);
//float lat, lon, alt;

OneWire onewire(2); //Se establece el pin 2 para el sensor de temperaura
DallasTemperature temp_sensor(&onewire);

//conexión al módulo BT
int Rx = 10; 
int Tx = 11;
SoftwareSerial moduleBtLE(Rx,Tx);
SoftwareSerial gpsSerial(5,6);

//SENSORES
const uint8_t sensors_buff = 5;
char selected_sensors[sensors_buff] = {0,0,0,0,0};
String opc = "";
int cant_medi = "";
int time = "";

//turbidez
//const int turbPin = A3;

//pH
const int pHpin = A2;
//float Po;
unsigned long int avgValue;
float calibration = 24.50;
int buf[10], temp;

//TDS
GravityTDS gravityTds;
float tdsValue = 0,temperature = 25;

//OD
float SOD_value;

void setup() {
  // put your setup code here, to run once:

  moduleBtLE.begin(9600);
  temp_sensor.begin(); // preparar la libreria dallas temperature
  Serial.begin(9600);
  gpsSerial.begin(9600);
  //ss.begin(9600);
  /*while (!Serial) {
  }// espera conexión
  */

  //delay(1000);

  DO.begin(); //inicializar sensor DO

  gravityTds.setPin(tdsPin);
  gravityTds.setAref(5.0);  //Voltaje de referencia del ADC, default 5.0V en Arduino UNO
  gravityTds.setAdcRange(1024);  //1024 for 10bit ADC; 4096 for 12bit ADC
  gravityTds.begin();  //inicializacion sensor TDS
  
}

void loop() {
  // put your main code here, to run repeatedly:

  if(moduleBtLE.available() > 0){ //si el la conexion bt esta disponible

    //moduleBtLE.println("Listo el dipositivo!");
    String data_in = moduleBtLE.readString();
  
    Serial.println(data_in);
    //moduleBtLE.println(data_in);
    dataSplit(data_in);

    if (opc.equals("CAL")){ // para calibrar

      //moduleBtLE.print("OPCION: ");
      //moduleBtLE.println(opc);
      //moduleBtLE.print("SENSORES: ");
      //moduleBtLE.println(sensors_str);

      moduleBtLE.println("INICIANDO CALIBRACION");
      
      for (int i = 0;  i < sensors_buff; i++){

        if (selected_sensors[i] == '1'){

          if (i == 2){
            //String known_tdsValue = moduleBtLE.readString();
            String known_tdsValue = "500";
            moduleBtLE.println("Iniciando calibración del sensor de SOLIDOS DISUELTOS TOTALES");
            //moduleBtLE.println("SIGA LAS SIGUIENTES INSTRUCCIONES: \n1.Limpie la sonda TDS, luego séquela con papel absorbente\n2.Inserte la sonda en la solución tampón de conductividad eléctrica conocida o valor TDS, luego revuelva suavemente y espere lecturas estables\nSi no tiene la solución tampón estándar, un boligrafo TDS también puede medir el valor TDS de la solución líquida.");
            tdsValue = -1;
            while(tdsValue != known_tdsValue.toFloat()){
              temp_sensor.requestTemperatures();
              float temperature = temp_sensor.getTempCByIndex(0); //add your temperature sensor and read it
              gravityTds.setTemperature(temperature);  // set the temperature and execute temperature compensation
              gravityTds.update();  //sample and calculate
              tdsValue = gravityTds.getTdsValue();  // then get the value
              moduleBtLE.print(tdsValue,2);
              moduleBtLE.println("mg/l");
              delay(1000);
            }
            moduleBtLE.println("Calibración lista");
          }

          if (i == 3){
            moduleBtLE.println("Iniciando calibración del sensor de PH");
            //moduleBtLE.println("ADVERTENCIA!!: Este proceso tarda aproximadamente 10 minutos");
            unsigned long start_time = millis();
            unsigned long now_time = millis();

            while( (now_time - start_time) < 600000){
              for(int i=0;i<10;i++){
                buf[i] = analogRead(pHpin);
                delay(30);
              }
              for(int i=0;i<9;i++){
                for(int j = i+1;j<10;j++){
                  if(buf[i] > buf[j]){
                    temp = buf[i];
                    buf[i] = buf[j];
                    buf[j] = temp;
                  }
                }
              }
              avgValue = 0;
              for(int i=2;i<8;i++){
                avgValue += buf[i];
              }
              float pHVol= (float)avgValue *5.0 / 1024 / 6;
              float phValue = -5.70 * pHVol + calibration;

              moduleBtLE.println(phValue);

              delay(500);
              now_time = millis();
            }
            moduleBtLE.println("Calibración lista");
          }

          if (i == 4){
            moduleBtLE.println("Iniciando calibración del dispositivo de SATURACION DE OXIGENO DISUELTO");
            DO.cal_clear();
            DO.cal();
            moduleBtLE.print(DO.read_do_percentage());
            moduleBtLE.println("%");
            moduleBtLE.println("Calibración lista");
          }
        }
      }
    }

    if (opc.equals("INI")){

      String data_out = "";

      //data_out.concat(getCoord());

      data_out.concat("$DATOS/");

      for (int i = 0;  i < sensors_buff; i++){
        if (selected_sensors[i] == '1'){
          if (i == 0){
            moduleBtLE.println("Midiendo Turbidez...");
            
            data_out.concat("TURBIDEZ");
            for (int x = 0; x < cant_medi; x++){
              //INSTRUCCIONES PARA MEDIR
              int turbPin = analogRead(A3);// read the input on analog pin 0:
              float voltage = turbPin * (5.0 / 1024.0); // Convert the analog reading (which goes from 0 – 1023) to a voltage (0 – 5V):
              float ntu = voltage*(-865.68)+1679.4192;
              if(ntu<0){
                ntu=0;  
              }
              float suma=0;
              for(int i=0; i<30; i++){
                  suma=suma +ntu;
                  //Serial.println("NTU");
              }
              ntu = suma/30;

              moduleBtLE.print(round(ntu));
              moduleBtLE.println("NTU");

              data_out.concat(",");
              data_out.concat(round(ntu));
              delay(time*1000);
            }
            data_out.concat("/");
            //moduleBtLE.println(data_out);
          }

          if (i == 1){
            moduleBtLE.println("Midiendo Temperatura...");
            
            data_out.concat("TEMPERATURA");
            for (int x = 0; x < cant_medi; x++){
              temp_sensor.requestTemperatures();
              float temp = temp_sensor.getTempCByIndex(0);

              moduleBtLE.print(temp);
              moduleBtLE.println("°C");

              data_out.concat(",");
              data_out.concat(temp);
              delay(time*1000);
            }
            data_out.concat("/");
            //moduleBtLE.println(data_out);
          }

          if (i == 2){
            moduleBtLE.println("Midiendo TDS...");
            moduleBtLE.println("ADVERTENCIA: La sonda no puede estar demasiado cerca de objetos, de lo contrario afectará la lectura.");
            
            data_out.concat("TDS");
            for (int x = 0; x < cant_medi; x++){
              //INSTRUCCIONES PARA MEDIR
              temp_sensor.requestTemperatures();
              float temperature = temp_sensor.getTempCByIndex(0); //add your temperature sensor and read it
              gravityTds.setTemperature(temperature);  // set the temperature and execute temperature compensation
              gravityTds.update();  //sample and calculate
              tdsValue = gravityTds.getTdsValue();  // then get the value
              
              moduleBtLE.print(tdsValue,2);
              moduleBtLE.println("mg/l");
              
              data_out.concat(",");
              data_out.concat(tdsValue);
              delay(time*1000);
            }
            data_out.concat("/");
          }

          if (i == 3){
            moduleBtLE.println("Midiendo pH...");

            data_out.concat("PH");
            for (int x = 0; x < cant_medi; x++){
            //INSTRUCCIONES PARA MEDIR
              //Po = (1023 - analogRead(pHpin)) / 73.07; // Read and reverse the analogue input value from the pH sensor then scale 0-14.
              //float ph = analogRead(pHpin) * (5.0 / 1023.0)
              for(int i=0;i<10;i++){
                buf[i] = analogRead(pHpin);
                delay(30);
              }
              for(int i=0;i<9;i++){
                for(int j = i+1;j<10;j++){
                  if(buf[i] > buf[j]){
                    temp = buf[i];
                    buf[i] = buf[j];
                    buf[j] = temp;
                  }
                }
              }

              avgValue = 0;
              for(int i=2;i<8;i++){
                avgValue += buf[i];
              }

              float pHVol= (float)avgValue *5.0 / 1024 / 6;
              float phValue = -5.70 * pHVol + calibration;

              moduleBtLE.println(phValue, 2);

              data_out.concat(",");
              data_out.concat(phValue);
              
              delay(time*1000);
            }
            data_out.concat("/");
          }

          if (i == 4){
            moduleBtLE.println("Midiendo Saturación de Oxígeno Disuelto...");

            data_out.concat("OD");
            for (int x = 0; x < cant_medi; x++){
            //INSTRUCCIONES PARA MEDIR
              SOD_value = DO.read_do_percentage();
              moduleBtLE.print(SOD_value);
              moduleBtLE.println("%");

              data_out.concat(",");
              data_out.concat(SOD_value);
              delay(time*1000);
            }
            data_out.concat("/");
          }
        }
      }
      //data_out.concat("$/END");
      moduleBtLE.print(data_out);
      Serial.println(data_out);
    }
  }

  gpsSerial.listen();
  // Si hay datos que entran, leerlos 
  // y enviarlos al puerto serie hardware:
  Serial.println("Datos del GPS:");
  while (gpsSerial.available() > 0) {
    float latitude, longitude, altitude;
    gps.f_get_position(&latitude, &longitude);
    altitude = gps.f_altitude();
    Serial.print("LAT: ");
    Serial.println(latitude,6);
    Serial.print("LONG: ");
    Serial.println(longitude,6);
    Serial.print("ALT: ");
    Serial.println(altitude);
  }
}


void dataSplit(String str){
  str.trim();
  String aux = "";

  int index_lim = str.indexOf(':');
  opc = str.substring(0,index_lim);
  str.remove(0,index_lim+1);
  ///moduleBtLE.println("OPCION:");
  ///moduleBtLE.println(opc);
  //moduleBtLE.println("NUEVA CADENA");
  //moduleBtLE.println(str);

  index_lim = str.indexOf(':');
  aux = str.substring(0,index_lim);
  str.remove(0,index_lim+1);
  ///moduleBtLE.print("CANTIDAD: ");
  ///moduleBtLE.println(aux);
  cant_medi = aux.toInt();
  

  index_lim = str.indexOf(':');
  aux = str.substring(0,index_lim);
  str.remove(0,index_lim+1);
  ///moduleBtLE.print("TIEMPO: ");
  ///moduleBtLE.println(aux);
  time = aux.toInt();

  str.toCharArray(selected_sensors,sensors_buff+1);

}
