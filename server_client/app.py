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
import json
import os
import logging
from datetime import datetime


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
DATA_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "last_message.txt")
LOG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "log.log")

# Global Variables
mqtt_client: mqtt.Client = mqtt.Client()
logs: list[str] = []
to_display: str = ""
command_to_send: str = ""

# Logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S',
    handlers=[
        logging.FileHandler(LOG_FILE),
        logging.StreamHandler()
    ]
)

def log_event(message: str) -> None:
    # Log to file (logging module handles its own timestamp)
    logging.info(message)
    
    # Log to in-memory list for UI
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    logs.append(f"[{timestamp}] {message}")


# Helper functions
def load_last_message() -> str:
    if os.path.exists(DATA_FILE):
        try:
            with open(DATA_FILE, 'r') as file:
                return file.read()
        except Exception as e:
            log_event(f"Error loading last message: {e}")
    return ""

def save_last_message(message: str):
    try:
        with open(DATA_FILE, 'w') as file:
            file.write(message)
    except Exception as e:
        log_event(f"Error saving last message: {e}")

def format_display_box(display_text: str) -> str:
    lines = display_text.split('\n')
    box = "\n   ---------------------------\n"
    for line in lines:
        box += f"   | {line:<25} |\n"
    box += "   ---------------------------"
    return box


# MQTT Callbacks
def on_connect(client: mqtt.Client, userdata: dict, flags: dict, err: int):
    if err == 0:
        log_event("Connected to MQTT Broker!")
        client.subscribe(STATUS_TOPIC)
    else:
        log_event(f"Failed to connect, return code {err}")

def on_message(client: mqtt.Client, userdata: dict, msg: mqtt.MQTTMessage):
    global command_to_send  # Add this line to access the global variable

    if msg.topic == STATUS_TOPIC:
        try:
            payload_str = msg.payload.decode()

            sanitized_payload = payload_str.replace('\n', '\\n')
            
            data = json.loads(sanitized_payload, strict=False)
            
            # Format log entry
            formatted_log = "{\n"
            if "status" in data:
                formatted_log += f'    "status": "{data["status"]}",\n'
            if "uptime" in data:
                formatted_log += f'    "uptime": {data["uptime"]},\n'
            
            if "display" in data:
                formatted_log += f'    "display": {format_display_box(data["display"])}\n'
            
            formatted_log += "}"
            log_event(f"Received:\n{formatted_log}")
            
            if data.get("status") == "online":
                log_event(f"Setting display message to: {format_display_box(to_display)}")
                mqtt_client.publish(DISPLAY_TOPIC, to_display)
                if command_to_send != "":
                    log_event(f"Sending command: {command_to_send}")
                    mqtt_client.publish(COMMAND_TOPIC, command_to_send)
                    command_to_send = ""
                
        except json.JSONDecodeError:
            log_event(f"Failed to decode JSON: {msg.payload}")
            log_event(msg.payload.decode())
        except Exception as e:
            log_event(f"Error in on_message: {e}")


# Flask Routes
@app.route('/')
def index()-> str:
    return render_template('index.html', to_display=to_display, command_to_send=command_to_send)

@app.route('/get_display_message', methods=['POST'])
def get_display_message()-> str:
    try:
        data = request.json
        message: str = data.get('message', '')
        global to_display
        to_display = message
        save_last_message(message)
        log_event(f"Updated display message: {format_display_box(message)}")
        return jsonify({"success": True, "message": "Display message updated"})
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/get_command', methods=['POST'])
def get_command()-> str:
    try:
        data = request.json
        commmand: str = data.get('command', '')
        global command_to_send
        command_to_send = commmand
        if commmand == "":
            log_event("Cleared queued command.")
        else:
            log_event(f"Queued command: {commmand}")
        return jsonify({"success": True, "message": f"Command queued: {commmand}"})
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/get_logs', methods=['GET'])
def get_logs()-> str:
    return jsonify({"logs": logs, "command_to_send": command_to_send})

@app.route('/clear_logs', methods=['POST'])
def clear_logs():
    global logs
    logs.clear()
    return jsonify({"success": True})



if __name__ == '__main__':

    to_display = load_last_message()

    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_start()
    except Exception as e:
        log_event(f"Failed to connect to MQTT broker: {e}")

    app.run(debug=False, host=HOST, port=PORT)