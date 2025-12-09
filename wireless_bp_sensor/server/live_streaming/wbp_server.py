from flask import Flask, send_from_directory, jsonify, request
from flask_sock import Sock
import threading
import json
from datetime import datetime
import os

app = Flask(__name__)
sock = Sock(app)

# Store all connected browser sockets
browser_clients = set()
lock = threading.Lock()

# Global to hold the single ESP connection
esp_client = None 

EVENT_FILE = "events.json"

if not os.path.exists(EVENT_FILE):
    with open(EVENT_FILE, "w") as f:
        json.dump([], f)

@app.route('/')
def index():
    return send_from_directory('.', 'index.html')

@app.route("/resonance", methods=["POST"])
def save_resonance_data():
    payload = request.get_json()
    if not payload or "points" not in payload:
        return jsonify({"error": "Missing points[]"}), 400

    timestamp = datetime.utcnow().isoformat() + "Z"
    
    # Load, Append, Save Logic (unchanged)
    with open(EVENT_FILE, "r") as f:
        events = json.load(f)
    new_id = (events[-1]["id"] + 1) if events else 1
    event = { "id": new_id, "timestamp": timestamp, "points": payload["points"] }
    events.append(event)
    with open(EVENT_FILE, "w") as f:
        json.dump(events, f, indent=2)

    print(f"[DATA] Stored event {new_id}")

    # Notify browsers
    with lock:
        dead = []
        for client in browser_clients:
            try:
                client.send(f"EVENT_NEW,{new_id}")
            except:
                dead.append(client)
        for d in dead:
            browser_clients.remove(d)

    return jsonify({"status": "ok", "event_id": new_id})

@app.route("/events", methods=["GET"])
def list_events():
    with open(EVENT_FILE, "r") as f:
        events = json.load(f)
    summary = [{"id": e["id"], "timestamp": e["timestamp"]} for e in events]
    return jsonify(summary)

@app.route("/events/<int:event_id>", methods=["GET"])
def get_event(event_id):
    with open(EVENT_FILE, "r") as f:
        events = json.load(f)
    for e in events:
        if e["id"] == event_id:
            return jsonify(e)
    return jsonify({"error": "Event not found"}), 404


# -----------------------
# WS: ESP → Server
# -----------------------
@sock.route('/esp')
def esp_socket(ws):
    global esp_client
    print("[ESP] Connected")
    
    # Register this socket as the active ESP
    esp_client = ws 

    while True:
        try:
            msg = ws.receive()
            if msg is None:
                break
            
            # Broadcast ESP data to all Browsers
            with lock:
                dead = []
                for client in browser_clients:
                    try:
                        client.send(msg)
                    except:
                        dead.append(client)
                for d in dead:
                    browser_clients.remove(d)

        except Exception as e:
            print("[ESP] Error:", e)
            break

    print("[ESP] Disconnected")
    esp_client = None # Clear global on disconnect


# -----------------------
# WS: Browser → Server
# -----------------------
@sock.route('/stream')
def stream_socket(ws):
    print("[Browser] Connected")
    with lock:
        browser_clients.add(ws)

    while True:
        try:
            # Wait for message from Browser
            msg = ws.receive() 
            if msg is None:
                break
            
            # Forward "TOGGLE" command to ESP if connected
            if msg == "TOGGLE":
                if esp_client:
                    try:
                        esp_client.send("TOGGLE")
                        print("[CMD] Forwarded TOGGLE to ESP")
                    except Exception as e:
                        print("[CMD] Failed to send to ESP:", e)
                else:
                    print("[CMD] No ESP connected to receive TOGGLE")

        except:
            break

    with lock:
        browser_clients.remove(ws)
    print("[Browser] Disconnected")

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000)
