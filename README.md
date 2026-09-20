# MSP430 Telemetry Dashboard

A live operator dashboard for an ultra-low-power sensor node — the browser layer of a firmware → host-service → web pipeline. This repo ships the **front-end** with a built-in telemetry simulator, so it runs and demos with no hardware attached.

Built as the "centerpiece" project from my job-search plan: it proves the claim the resume makes — that I can design an operator interface and build it, the same work I do for real instruments, on the web stack.

---

## The Pipeline It Represents

```
MSP430FR6989          Python host             This dashboard
C firmware   ─UART→   pyserial + FastAPI  ─WS→  Canvas UI
(sensor +            (validate, timestamp,      (live chart,
 segment LCD)         SQLite, WebSocket)         alarms, log)
```

The `js/simulator.js` module stands in for the firmware + host service. It emits **framed readings** — `{ seq, sensor, unit, value, t }` — the exact shape a real WebSocket feed would forward, so swapping in a live socket later touches one function.

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
| HTML5 | Structure |
| CSS3 | Dark operator-console styling, responsive layout |
| JavaScript | State machine, alarm logic, telemetry simulator |
| Canvas API | Custom strip chart (no libraries) |

No frameworks, no build step. Open `index.html` and press **Start**.

---

## Files

```
index.html          layout
css/styles.css      operator-console theme
js/simulator.js     telemetry source (stands in for firmware + host)
js/chart.js         Canvas strip chart with threshold bands
js/app.js           loop, alarm state machine, rendering
```

## Next

- Replace the simulator with a real WebSocket to a FastAPI host reading an MSP430 over UART
- Push calibration settings back down to the device
- Persist history to SQLite via the host service

---

## Author

Isabella Gallo — [bg1750.github.io/bellagallo](https://bg1750.github.io/bellagallo)
