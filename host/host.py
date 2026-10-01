"""
host.py — bridges the MSP430 serial stream to the web dashboard.

Reads framed readings from the LaunchPad over UART (pyserial), validates the
checksum, timestamps and stores each in SQLite, and broadcasts valid readings
to any connected dashboards over a WebSocket. Bad/partial frames are dropped
and counted, never fatal.

Run:
    python host.py --port COM6                 # real board (Windows) — use the
                                               #   "MSP Application UART1" COM port;
                                               #   the number varies by USB port.
    python host.py --port /dev/ttyACM1         # real board (Linux/Mac)
    python host.py --sim                        # no hardware: generate frames

Then open the dashboard and switch its source to "Device".
See ../PROTOCOL.md for the wire format.
"""
import argparse
import asyncio
import json
import random
import sqlite3
import threading
import time

import serial  # pyserial
import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from protocol import parse_frame  # pure frame parsing (see protocol.py)


# ---------------------------------------------------------------- storage
def init_db(path="telemetry.db"):
    con = sqlite3.connect(path, check_same_thread=False)
    con.execute(
        "CREATE TABLE IF NOT EXISTS readings "
        "(seq INTEGER, sensor TEXT, unit TEXT, value REAL, t INTEGER)"
    )
    con.commit()
    return con


# ---------------------------------------------------------------- ws hub
class Hub:
    def __init__(self):
        self.clients: set[WebSocket] = set()

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self.clients.add(ws)

    def disconnect(self, ws: WebSocket):
        self.clients.discard(ws)

    async def broadcast(self, message: str):
        for ws in list(self.clients):
            try:
                await ws.send_text(message)
            except Exception:
                self.clients.discard(ws)


# ---------------------------------------------------------------- app
app = FastAPI(title="MSP430 Telemetry Host")
app.add_middleware(
    CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"]
)

hub = Hub()
db = init_db()
stats = {"ok": 0, "bad": 0, "started": time.time()}
_stop = threading.Event()


@app.get("/")
def status():
    return {
        "service": "msp430-telemetry-host",
        "frames_ok": stats["ok"],
        "frames_bad": stats["bad"],
        "uptime_s": round(time.time() - stats["started"], 1),
        "clients": len(hub.clients),
    }


@app.get("/history")
def history(limit: int = 100):
    rows = db.execute(
        "SELECT seq, sensor, unit, value, t FROM readings ORDER BY t DESC LIMIT ?",
        (limit,),
    ).fetchall()
    cols = ["seq", "sensor", "unit", "value", "t"]
    return [dict(zip(cols, r)) for r in reversed(rows)]


@app.websocket("/ws")
async def ws_endpoint(ws: WebSocket):
    await hub.connect(ws)
    try:
        while True:
            await ws.receive_text()  # keep the connection open
    except WebSocketDisconnect:
        hub.disconnect(ws)


# ---------------------------------------------------------------- ingest
def handle_reading(frame: dict, loop: asyncio.AbstractEventLoop):
    frame["t"] = int(time.time() * 1000)
    db.execute(
        "INSERT INTO readings VALUES (?,?,?,?,?)",
        (frame["seq"], frame["sensor"], frame["unit"], frame["value"], frame["t"]),
    )
    db.commit()
    stats["ok"] += 1
    asyncio.run_coroutine_threadsafe(hub.broadcast(json.dumps(frame)), loop)


def serial_reader(port: str, baud: int, loop: asyncio.AbstractEventLoop):
    """Blocking read loop, run in a background thread."""
    print(f"[host] opening {port} @ {baud} …")
    while not _stop.is_set():
        try:
            with serial.Serial(port, baud, timeout=1) as ser:
                print("[host] connected")
                while not _stop.is_set():
                    raw = ser.readline().decode("ascii", errors="replace")
                    if not raw:
                        continue
                    frame = parse_frame(raw)
                    if frame is None:
                        stats["bad"] += 1
                        continue
                    handle_reading(frame, loop)
        except serial.SerialException as e:
            print(f"[host] serial error: {e} — retrying in 2 s")
            time.sleep(2)


def sim_reader(loop: asyncio.AbstractEventLoop):
    """Generate frames when no board is attached (--sim)."""
    print("[host] simulation mode — no hardware")
    seq, value = 0, 22.0
    while not _stop.is_set():
        value += (random.random() - 0.5) * 0.6
        value = max(-5, min(60, value))
        handle_reading(
            {"seq": seq, "sensor": "temp", "unit": "°C", "value": round(value, 2)}, loop
        )
        seq += 1
        time.sleep(0.5)


@app.on_event("startup")
async def start_reader():
    loop = asyncio.get_running_loop()
    target = sim_reader if app.state.sim else serial_reader
    args = (loop,) if app.state.sim else (app.state.port, app.state.baud, loop)
    threading.Thread(target=target, args=args, daemon=True).start()


@app.on_event("shutdown")
def stop_reader():
    _stop.set()


# ---------------------------------------------------------------- main
if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="MSP430 telemetry host")
    ap.add_argument("--port", help="serial port, e.g. COM6 or /dev/ttyACM1")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--sim", action="store_true", help="simulate, no hardware")
    ap.add_argument("--http-port", type=int, default=8000)
    args = ap.parse_args()

    if not args.sim and not args.port:
        ap.error("provide --port, or use --sim to run without hardware")

    app.state.sim = args.sim
    app.state.port = args.port
    app.state.baud = args.baud

    uvicorn.run(app, host="0.0.0.0", port=args.http_port)
