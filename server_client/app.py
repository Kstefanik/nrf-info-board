# *****************************************************************************
# * @file    app.py
# * @author  Karol Stefanik
# * @brief   Flask application for nrf-info-board server client
# *
# * Copyright (c) 2025 Karol Stefanik
# * SPDX-License-Identifier: Apache-2.0
# *****************************************************************************#



from flask import Flask, render_template, request, jsonify
import paho.mqtt.client as mqtt

app = Flask(__name__)

# MQTT Configuration
MQTT_BROKER = "mqtt.nordicsemi.academy"
MQTT_PORT = 1883
DISPLAY_TOPIC = "info-board/display"
COMMAND_TOPIC = "info-board/command"
STATUS_TOPIC = "info-board/status"

# Server Configuration
PORT = 5000
HOST = '0.0.0.0'

# MQTT Client Setup
mqtt_client: mqtt.Client = mqtt.Client()
logs: list[str] = []

def on_connect(client: mqtt.Client, userdata: dict, flags: dict, err: int):
    if err == 0:
        print("Connected to MQTT Broker!")
        client.subscribe(STATUS_TOPIC)
    else:
        print(f"Failed to connect, return code {err}")

def on_message(client: mqtt.Client, userdata: dict, msg: mqtt.MQTTMessage):
    if msg.topic == STATUS_TOPIC:
        logs.append(msg.payload.decode())
        # Limit log size
        if len(logs) > 1000:
            logs.pop(0)

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

def connect_mqtt():
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_start()
    except Exception as e:
        print(f"Failed to connect to MQTT broker: {e}")

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/send_message', methods=['POST'])
def send_message():
    try:
        data = request.json
        message = data.get('message')
        topic = data.get('topic', DISPLAY_TOPIC)
        
        if not message:
            return jsonify({"error": "Message is required"}), 400
        
        print(message)
        
        result = mqtt_client.publish(topic, message)
        
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            return jsonify({"success": True, "message": "Command sent successfully"})
        else:
            return jsonify({"error": "Failed to send command"}), 500
            
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/get_logs', methods=['GET'])
def get_logs():
    return jsonify({"logs": logs})

@app.route('/clear_logs', methods=['POST'])
def clear_logs():
    global logs
    logs.clear()
    return jsonify({"success": True})


if __name__ == '__main__':
    connect_mqtt()
    app.run(debug=True, host=HOST, port=PORT)