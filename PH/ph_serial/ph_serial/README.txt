Dentro de este código se hace la implementación del sensor de pH genérico de Arduino
de manera unitaria por lo que no se hace la configuración para cualquier otro sensor

Este código funciona tanto para el uso del monitor serial del Arduino IDE así como 
con el uso de Flask para usarlo desde un navegador web, para hacer la conexión entre
el Arduino e internet en este caso con el servidor de Flask se debe hacer uso de un 
ESP01, el ESP01 cuenta con su propio código el cuál se encuentra en "Conf_esp01"

Este código recibe en texto plano los comandos y los ejecuta en el monitor serial del
ya mencionado Arduino IDE, el resultado de los datos recabados por el sensor se 
entregan de igual manera en texto plano al ESP01. Así mismo se tiene la funcionalidad
básica y normal de la comunicación con el sensor siendo que se envían y reciben datos
del mismo, los datos que se envían son aquellos necesarios para la calibración.

Dentro de los comandos que se pueden y deben ejecutarse para el funcionamiento del 
sensor se encuentra:

- Start
Este comando inicia la medición continua del sensor la cuál toma el promedio 
después de 10 mediciones que toma, el resultado que se ve dentro del mismo monitor 
serial es el del promedio

- Stop
Detiene las mediciones continuas del sensor

- Calibrate X.XX
Como su nombre indica es el comando necesario para la calibración del sensor, este
comando además recibe el valor del pH por el cuál se va a calibrar, con esto nos 
referimos a la solución por la cuál se hará la calibración la cuál ya tiene un valor
establecido y comprobado, el valor debe estar dado por un entero y dos decimales
por ejemplo "7.00" 