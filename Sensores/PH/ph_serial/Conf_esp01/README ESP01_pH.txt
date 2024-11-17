Dentro del presente código tenemos la configuración necesaria para el ESP01
para servir y comunicar los datos del código  para el sensor de pH. El 
código sirve de tal manera que obtiene valores JSON del servidor Flask y lo
convierte en texto plano para el Arduino, de igual manera se busca que el
texto plano entregado por el Arduino se convierta a JSON para el servidor 
de Flask pueda ser mostrado de manera dinámica.

El ESP01 debe ser programado de manera individual al Arduino, ya que 
contiene su propio código, esto se puede hacer mediante conectar los puertos
seriales del Arduino y del ESP01 como sería el TX0 y el RX0, para que el 
ESP01 entre en modo de programación se debe poner a tierra (GND) el pin
GPIO0 del ESP01 para que se quede la configuración guardada dentro del mismo
dispositivo y funcione de manera adecuada. El ESP01 debe ser conectado a una
fuente de voltaje de 3.3V es importante tener en cuenta que con 5V se quema y
deja de funcionar.
Por recomendación se pide que se haga un uso del RESET del mismo dispositivo 
antes de la programación y después de mismo.

Este código hasta el momento no solo sirve para la configuración del 
dispositivo de tal manera que solo comunique la información del Arduino a
Flask y de manera inversa, sino que también tiene la funcionalidad de 
convertir de JSON a texto plano la información recibida por cada parte

Implementaciones a realizar:
Que solo comunique la información en formato JSON ya que el convertirlo a
texto plano dificulta el proceso, puede alentar un poco el dispositivo así
como carga bastante de "estrés" al ESP

Hacer que el ESP01 funcione como un servidor y obtenga una dirección IP
propia para después exponerla a internet haciendo uso de cualquier otra 
tecnología, por lo que aunque reciba parte de las peticiones debe 
comunicarlas ante el Arduino