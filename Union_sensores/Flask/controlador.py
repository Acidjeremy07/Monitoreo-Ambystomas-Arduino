from flask import Flask, render_template, request
from flask import jsonify
import os
import threading
from pyngrok import ngrok
import requests

app = Flask(__name__)

# port = 5000

# # Open a ngrok tunnel to the HTTP server
# print("AQUI SI LLEGA 1")
# public_url = ngrok.connect(port)
# print("AQUI SI LLEGA 2")

# print(" * ngrok tunnel", public_url)

# # Update any base URLs to use the public ngrok URL
# app.config["BASE_URL"] = public_url

@app.route('/')
def principal():
    requests.get('http://192.168.0.110/')
    return render_template("index.html")

if __name__ =="__main__":
    app.run()