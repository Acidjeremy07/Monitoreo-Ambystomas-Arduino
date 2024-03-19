const int PHPin = A0;  //Sensor conectado en PIN A0

//𝒚=−𝟓.𝟕𝒙+𝟐𝟏.𝟒
//y = -9.53x + 42.59  nuestro valor

const float b = 38.308;  //Constante de la ecuación (Y = mx + b)
const float m = -9.53;  //Pendiente de la recta (Y = mx + b)

void setup() {
  Serial.begin(9600);
}


void loop() {

  PHpromedio();  //función que calcula el ph promedio de 10 muestras
  delay(1000);
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