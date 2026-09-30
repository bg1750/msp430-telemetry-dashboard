# MSP430 Telemetry Dashboard

A complete **hardware-to-browser** telemetry system for an ultra-low-power sensor node: C firmware on an MSP430FR6989 streams sensor readings, a Python service validates and stores them, and a live web dashboard renders them. It runs end-to-end with the board attached and still demos with a **built-in simulator** when it isn't.


---

## The Pipeline

```
MSP430FR6989            Python host              This dashboard
C firmware     ─UART→   pyserial + FastAPI  ─WS→  Canvas UI
(temp sensor +          (validate checksum,       (live chart,
 segment LCD)            timestamp, SQLite,        alarms, log)
                         WebSocket broadcast)
```

| Layer | Folder | Language | What it does |
|---|---|---|---|
| Firmware | [`firmware/`](firmware/) | C | Reads the internal temp sensor, drives the segment LCD, streams framed readings over UART. Sleeps in LPM3. |
| Host | [`host/`](host/) | Python | `pyserial` reads frames, validates the checksum, stores to SQLite, serves a FastAPI WebSocket. |
| Dashboard | `js/`, `index.html` | JavaScript | Subscribes to the WebSocket; Canvas chart, alarm logic, event log. |

The wire format between firmware and host is documented in [PROTOCOL.md](PROTOCOL.md).

### Three sources

The dashboard has three data-source buttons:

- **Device** — connects to the Python host over WebSocket (`ws://localhost:8000/ws`) for the real hardware stream.
- **Live (simulated)** — `js/simulator.js` generates a random-walk sensor, so the front-end runs with no board or host.
- **Replay** — plays a recorded run so a demo repeats identically.

### Run the full pipeline

1. Flash the firmware — see [firmware/README.md](firmware/README.md).
2. Start the host — `cd host && python host.py --port COM5` (or `--sim` for no hardware). See [host/README.md](host/README.md).
3. Serve the dashboard — `npx http-server -c-1 .` — click **Device** → **Start**.

---

## What It Does

- **Live & Replay modes** — "Live" streams a simulated random-walk sensor; "Replay" plays a recorded run so a demo repeats identically
- **Three sensors** — temperature, light, and supply voltage, each with sensible default alarm thresholds
- **Stat tiles** — current value, min/max, and running mean
- **Canvas strip chart** — hand-rolled, no charting library: high-DPI aware, responsive, with shaded high/low threshold bands
- **Alarm logic** — nominal / near-limit / alarm, with adjustable low and high thresholds
- **Event log** — logs only on state *transitions* (alarm entered, recovered), not every frame

---

## Handling the Ugly Cases

The alarm path is deliberately exercised: the simulator injects rare spikes so you can watch the status tile flip to **HIGH/LOW alarm** and the log record the transition and recovery — the "safety-critical mindset" signal an instrumentation employer screens for.

---

## Built With

| Technology | Role |
|---|---|
| C (MSP430) | Firmware: sensor sampling, LCD, UART, low-power modes |
| Python | Host: pyserial, FastAPI, WebSocket, SQLite |
| HTML5 / CSS3 | Dashboard structure and dark operator-console styling |
| JavaScript | State machine, alarm logic, WebSocket client, simulator |
| Canvas API | Custom strip chart (no libraries) |

The dashboard has no frameworks or build step. Its scripts load as ES modules, so serve the folder and press **Start**:

```
npx http-server -c-1 .      # then visit the printed localhost URL
```

---

## Testing

| Layer | Tool | Command |
|---|---|---|
| Unit | Vitest | `npm test` |
| e2e + accessibility | Playwright + axe-core | `npm run test:e2e` |

The unit suite covers the two pieces of logic worth locking down: the **alarm state machine** (nominal / near-limit / high / low, including exact-threshold behaviour) and the **telemetry framing** (sequence numbers, decimal precision per sensor, and range clamping over 500 samples). The e2e suite starts a stream and asserts the reading and event log update, then runs axe against the page. CI runs the unit suite on every push.

> First e2e run only: `npx playwright install`.

---

The unit suite covers the JavaScript logic. The **Python** frame parser has its own tests: `cd host && pytest test_host.py` (good frame, corrupted checksum, unknown tag, partial line).

## Files

```
firmware/               MSP430 C firmware (main.c, hal_LCD, Makefile, README)
host/                   Python host (host.py, protocol.py, tests, README)
PROTOCOL.md             the firmware↔host wire format
index.html              dashboard layout
css/styles.css          operator-console theme
js/simulator.js         built-in telemetry source, framing (pure, tested)
js/alarm.js             alarm state machine (pure, tested)
js/chart.js             Canvas strip chart with threshold bands
js/app.js               loop, WebSocket client, rendering
tests/                  Vitest unit + Playwright/axe e2e
```

## Next

- Push calibration settings from the dashboard back down to the device
- Add light / voltage sensors to the firmware (the protocol already carries their tags)
- Chart history replay from the host's SQLite store

---

## Author

Isabella Gallo — [bg1750.github.io/bellagallo](https://bg1750.github.io/bellagallo)
