# Host service — serial → WebSocket bridge

Python service that sits between the MSP430 firmware and the web dashboard.
Reads framed readings over UART, validates the checksum, timestamps and
stores each in SQLite, and broadcasts valid readings to connected dashboards
over a WebSocket.

## What it does

| Concern | How |
|---|---|
| Read | `pyserial` reads newline-framed lines from the LaunchPad |
| Validate | XOR checksum + field checks; bad/partial frames are **dropped and counted**, never fatal |
| Store | Every valid reading appended to `telemetry.db` (SQLite) |
| Serve | FastAPI: `GET /` status, `GET /history`, `WS /ws` live stream |

## Setup

```
cd host
python -m venv .venv
# Windows:  .venv\Scripts\activate
# Mac/Linux: source .venv/bin/activate
pip install -r requirements.txt
```

## Run

```
# real board — use the LaunchPad's Application/backchannel COM port
python host.py --port COM5            # Windows
python host.py --port /dev/ttyACM1    # Linux/Mac

# no hardware yet? generate frames so you can test the host + dashboard:
python host.py --sim
```

Find the port: Windows *Device Manager → Ports* ("MSP Application UART…");
Linux/Mac `ls /dev/tty*` (usually `ttyACM1`, the second of the two the board exposes).

## Endpoints

| Route | Purpose |
|---|---|
| `GET /` | Service status + `frames_ok` / `frames_bad` counters |
| `GET /history?limit=100` | Recent readings from SQLite (JSON) |
| `WS /ws` | Live reading stream the dashboard subscribes to |

Each WebSocket message:

```json
{ "seq": 142, "sensor": "temp", "unit": "°C", "value": 22.45, "t": 1758326400123 }
```

## Verify

1. Start the host (`--sim` is fine). It listens on `http://localhost:8000`.
2. Open `http://localhost:8000/` — you should see the frame counters rising.
3. Open the dashboard, click **Device** → **Start**; the chart fills from the live stream.

## Test the frame parser

`parse_frame()` is pure and unit-tested:

```
pip install pytest
pytest test_host.py
```

The tests cover a good frame, a corrupted checksum, an unknown tag, and a
partial line — the corruption cases from [../PROTOCOL.md](../PROTOCOL.md).
