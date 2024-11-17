from flask import Flask, request, jsonify, render_template

app = Flask(__name__)

last_ph = {}  # Variable global para almacenar los últimos datos de pH

@app.route('/command', methods=['POST', 'GET'])
def command():
    global last_command
    if request.method == 'POST':
        data = request.form.get('command')
        last_command = data  # Guardar el último comando recibido
        print("Received command:", data)
        return jsonify({"response": "Received command: {}".format(data)})
    elif request.method == 'GET':
        return jsonify({"command": last_command})  # Devolver el último comando al ESP01

@app.route('/data', methods=['POST'])
def receive_data():
    data_received = request.get_json()
    global last_ph
    last_ph = data_received  # Actualizar la última lectura de pH
    print("Received pH data:", data_received)
    return 'OK', 200

@app.route('/')
def index():
    return render_template('index.html', latest_ph=last_ph)  # Muestra la última lectura de pH

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
