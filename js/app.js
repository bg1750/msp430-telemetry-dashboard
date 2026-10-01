/* MSP430 Telemetry Dashboard — live WebSocket stream → tiles + chart + log.
   The only data source is the real device: the Python host forwards framed
   readings from the board over a WebSocket. No simulation, no replay. */
import { StripChart } from "./chart.js";
import { evaluateAlarm } from "./alarm.js";

(function () {
  "use strict";
  const $ = (id) => document.getElementById(id);

  const MAX_POINTS = 120;
  const HOST_WS = "ws://localhost:8000/ws"; // Python host (see host/README.md)

  const chart = StripChart($("chart"));

  let running = false;
  let ws = null;
  let data = [];
  let lastAlarm = "ok";

  /* ---- Reading intake ---- */
  // Single entry point for a reading from the live stream.
  function handleReading(r) {
    data.push(r);
    if (data.length > MAX_POINTS) data.shift();
    render(r);
  }

  /* ---- Live stream control ---- */
  function start() {
    if (running) return;
    running = true;
    $("startBtn").disabled = true;
    $("pauseBtn").disabled = false;
    connectDevice();
  }

  function pause() {
    running = false;
    disconnectDevice();
    setConnected(false);
    $("startBtn").disabled = false;
    $("pauseBtn").disabled = true;
    log("Stream paused", "");
  }

  /* ---- WebSocket to the Python host ---- */
  function connectDevice() {
    log("Connecting to host " + HOST_WS + " …", "");
    try {
      ws = new WebSocket(HOST_WS);
    } catch (e) {
      log("Could not open WebSocket — start host.py first", "alarm");
      return;
    }
    ws.onopen = () => { setConnected(true); log("Connected — streaming live telemetry", "ok"); };
    ws.onmessage = (ev) => {
      try {
        const r = JSON.parse(ev.data);
        if (typeof r.value === "number") handleReading(r);
      } catch (e) { log("Bad frame from host", "alarm"); }
    };
    ws.onerror = () => { log("WebSocket error — is host.py running on :8000?", "alarm"); };
    ws.onclose = () => { setConnected(false); if (running) log("Host disconnected", ""); };
  }

  function disconnectDevice() {
    if (ws) { ws.onclose = null; ws.close(); ws = null; }
  }

  /* ---- Render ---- */
  function render(r) {
    const unit = r.unit || "°C";
    const values = data.map((d) => d.value);
    const min = Math.min(...values);
    const max = Math.max(...values);
    const mean = values.reduce((a, b) => a + b, 0) / values.length;

    $("curVal").textContent = r.value.toFixed(1);
    $("curUnit").textContent = unit;
    $("minVal").textContent = min.toFixed(1);
    $("maxVal").textContent = max.toFixed(1);
    $("meanVal").textContent = mean.toFixed(1);

    const lo = parseFloat($("loThresh").value);
    const hi = parseFloat($("hiThresh").value);
    applyAlarm(r.value, lo, hi);

    chart.draw(data, { loThresh: lo, hiThresh: hi });
  }

  function applyAlarm(v, lo, hi) {
    const { state: stateNow, label } = evaluateAlarm(v, lo, hi);

    const tile = $("statusTile");
    const el = $("alarmState");
    tile.classList.remove("warn", "alarm");
    el.classList.remove("warn", "alarm", "ok");
    if (stateNow !== "ok") tile.classList.add(stateNow);
    el.classList.add(stateNow);
    el.textContent = label;

    // only log on state transitions, not every frame
    if (stateNow !== lastAlarm && stateNow !== "ok") {
      log(label + " — " + v.toFixed(2), stateNow);
    } else if (stateNow === "ok" && lastAlarm !== "ok") {
      log("Recovered to nominal", "ok");
    }
    lastAlarm = stateNow;
  }

  /* ---- Connection indicator ---- */
  function setConnected(on) {
    $("connText").textContent = on ? "Streaming" : (data.length ? "Paused" : "Disconnected");
    document.querySelector(".conn").classList.toggle("on", on);
  }

  /* ---- Log ---- */
  const logEl = $("log");
  function log(msg, cls) {
    const empty = logEl.querySelector(".log-empty");
    if (empty) empty.remove();
    const li = document.createElement("li");
    const t = new Date().toLocaleTimeString();
    li.innerHTML = `<span class="t">${t}</span><span class="m ${cls}">${msg}</span>`;
    logEl.prepend(li);
    while (logEl.children.length > 50) logEl.lastChild.remove();
  }
  function emptyLog() {
    logEl.innerHTML = '<li class="log-empty">No events yet — press Start.</li>';
  }

  /* ---- Events ---- */
  $("startBtn").addEventListener("click", start);
  $("pauseBtn").addEventListener("click", pause);
  $("clearLog").addEventListener("click", emptyLog);

  /* ---- Init ---- */
  emptyLog();
  chart.draw([], { loThresh: 10, hiThresh: 30 });
})();
