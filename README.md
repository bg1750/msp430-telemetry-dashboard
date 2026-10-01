# MSP430 Telemetry Dashboard

A complete **hardware-to-browser** telemetry system for an ultra-low-power sensor node: C firmware on an MSP430FR6989 streams temperature and supply-voltage readings, a Python service validates and stores them, and a live web dashboard renders them end-to-end. The dashboard shows **live hardware data only** — no simulation.


---

## The Pipeline

```
MSP430FR6989            Python host              This dashboard
C firmware     ─UART→   pyserial + FastAPI  ─WS→  Canvas UI
(temp + supply V,       (validate checksum,       (live chart,
 LED heartbeat)          timestamp, SQLite,        alarms, log)
                         WebSocket broadcast)
```

| Layer | Folder | Language | What it does |
|---|---|---|---|
| Firmware | [`firmware/`](firmware/) | C | Reads the internal temp sensor and supply-voltage monitor via ADC12_B and streams framed `TEMP`/`VOLT` readings over UART, with an LED heartbeat. Register-level, no driverlib. |
| Host | [`host/`](host/) | Python | `pyserial` reads frames, validates the checksum, stores to SQLite, serves a FastAPI WebSocket. |
| Dashboard | `js/`, `index.html` | JavaScript | Subscribes to the WebSocket; Canvas chart, alarm logic, event log. |

The wire format between firmware and host is documented in [PROTOCOL.md](PROTOCOL.md).

### Data source

The dashboard has a single source: the **live device**. Pressing **Start** opens a
WebSocket to the Python host (`ws://localhost:8000/ws`), which forwards each framed
reading from the board. There is no simulated or replayed data.

### Run the full pipeline

1. Flash the firmware — see [firmware/README.md](firmware/README.md).
2. Start the host — `cd host && python host.py --port COM6` (the "MSP Application UART1" port; the number varies). See [host/README.md](host/README.md).
3. Serve the dashboard — `npx http-server -c-1 .` — press **Start**.

---

## What It Does

- **Live stream** — subscribes to the host WebSocket and renders each reading as it arrives: temperature drives the chart/stats, supply voltage its own tile
- **Adjustable alarm thresholds** — set low/high limits; the status tile and chart bands reflect them live
- **Stat tiles** — current value, min/max, and running mean
- **Canvas strip chart** — hand-rolled, no charting library: high-DPI aware, responsive, with shaded high/low threshold bands
- **Alarm logic** — nominal / near-limit / alarm, with adjustable low and high thresholds
- **Event log** — logs only on state *transitions* (alarm entered, recovered), not every frame

---

## Handling the Ugly Cases

The host validates every frame against its XOR checksum and **silently drops** corrupt or partial ones — counted, never fatal — so unplugging the board mid-stream or injecting a bad byte never crashes the pipeline (see [PROTOCOL.md](PROTOCOL.md)). On the dashboard, when a reading crosses your low/high thresholds the status tile flips to **HIGH/LOW alarm** and the log records the transition and recovery — the "safety-critical mindset" signal an instrumentation employer screens for.

---

## Built With

| Technology | Role |
|---|---|
| C (MSP430) | Firmware: ADC sensor sampling, UART framing, register-level peripheral setup |
| Python | Host: pyserial, FastAPI, WebSocket, SQLite |
| HTML5 / CSS3 | Dashboard structure and dark operator-console styling |
| JavaScript | WebSocket client, alarm state machine, Canvas rendering |
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

The unit suite covers the **alarm state machine** (nominal / near-limit / high / low, including exact-threshold behaviour). The e2e suite asserts that pressing **Start** opens the live connection and updates the UI, then runs axe against the page. CI runs the unit suite on every push.

> First e2e run only: `npx playwright install`.

---

The unit suite covers the JavaScript logic. The **Python** frame parser has its own tests: `cd host && pytest test_host.py` (good frame, corrupted checksum, unknown tag, partial line).

## Files

```
firmware/               MSP430 C firmware (main.c, Makefile, README)
host/                   Python host (host.py, protocol.py, tests, README)
PROTOCOL.md             the firmware↔host wire format
index.html              dashboard layout
css/styles.css          operator-console theme
js/alarm.js             alarm state machine (pure, tested)
js/chart.js             Canvas strip chart with threshold bands
js/app.js               live WebSocket client, rendering, event log
tests/                  Vitest unit + Playwright/axe e2e
```

## Next

- Push calibration settings from the dashboard back down to the device
- Add light / voltage sensors to the firmware (the protocol already carries their tags)
- Chart history replay from the host's SQLite store

---

## Author

Isabella Gallo — [bg1750.github.io/bellagallo](https://bg1750.github.io/bellagallo)
