const int PHPin = A0;  //Sensor conectado en PIN A0


void setup() {
  Serial.begin(9600);
}


void loop() {

  voltajepromedio();  //función que calcula el ph promedio de 10 muestras
  delay(1000);
}




void impresion(float v) {

  Serial.print("Voltaje: ");
  Serial.println(v);

}


void voltajepromedio() {
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

  float Voltaje = muestrasprom * (5.0 / 1024.0);  //se calcula el voltaje


  impresion(Voltaje);
}