const int ODin = A0;  //Sensor conectado en PIN A0
float calibracion = 0;
int i = 0;

void setup() 
{
  Serial.begin(9600);
}

void loop() 
{
  if(i == 0) //10 segundos de calibración 
  {
    calibracion = voltajepromedio();
    i = i + 1;
  }
  oxigenodisuelto(calibracion); 
  delay(1000);
}

void impresion(float v) 
{
  Serial.print("Voltaje: ");
  Serial.println(v);
  Serial.print("mv");
}

void oxigenodisuelto(float vc)
{
  long muestras;       //Variable para las 10 muestras
  float muestrasprom;  //Variable para calcular el promedio de las muestras
  float porcentaje;

  muestras = 0;
  muestrasprom = 0;
  porcentaje = 0;

  //10 muestras, una cada 10ms
  for (int x = 0; x < 1000; x++) {
    muestras += analogRead(ODin);
    delay(10);
  }

  muestrasprom = muestras / 1000.0;

  float Voltaje = (muestrasprom * (5.0 / 1024.0) * 1000.0);
  porcentaje = (Voltaje / vc) * 100.0;
  Serial.print("Porcentaje de DO: ");
  Serial.println(porcentaje);
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

  float Voltaje = (muestrasprom * (5.0 / 1024.0) * 1000.0);  //se calcula el voltaje

  impresion(Voltaje);
  return Voltaje;
}