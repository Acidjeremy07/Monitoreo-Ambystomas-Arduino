void setup() {
Serial.begin(9600); //Baud rate: 9600
}
void loop() {

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
