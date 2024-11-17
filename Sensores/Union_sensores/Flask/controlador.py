from flask import Flask, render_template, request, jsonify
import requests

app = Flask(__name__)

@app.route('/')
def principal():
    response = requests.get('http://192.168.0.110/')  # Dirección IP del Arduino
    if response.status_code == 200:
        datos_sensores = response.json()  # Parsear la respuesta JSON
        return render_template("index.html", datos=datos_sensores)  # Pasar los datos a la plantilla
    else:
        return "Error al obtener los datos de los sensores", 500

if __name__ == "__main__":
    app.run()
