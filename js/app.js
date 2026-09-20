/* MSP430 Telemetry Dashboard — wiring: source → state → tiles + chart + log. */
import { StripChart } from "./chart.js";
import { Simulator, makeReplay, SENSORS } from "./simulator.js";
import { evaluateAlarm } from "./alarm.js";

(function () {
  "use strict";
  const $ = (id) => document.getElementById(id);

  const MAX_POINTS = 120;
  const INTERVAL = 500; // ms between readings

  const SENSOR_TITLES = { temp: "Temperature", light: "Light", voltage: "Supply voltage" };

  const chart = StripChart($("chart"));

  let mode = "live";       // "live" | "replay"
  let running = false;
  let timer = null;
  let data = [];
  let sim = null;
  let replayBuf = [];
  let replayIdx = 0;
  let lastAlarm = "ok";

  /* ---- Source management ---- */
  function currentSensor() { return $("sensorSel").value; }

  function resetSource() {
    data = [];
    if (mode === "live") {
      sim = Simulator(currentSensor());
    } else {
      replayBuf = makeReplay(currentSensor(), 240);
      replayIdx = 0;
    }
  }

  function nextReading() {
    if (mode === "live") return sim.next();
    const r = replayBuf[replayIdx % replayBuf.length];
    replayIdx++;
    return Object.assign({}, r, { t: Date.now() });
  }

  /* ---- Loop ---- */
  function tick() {
    const r = nextReading();
    data.push(r);
    if (data.length > MAX_POINTS) data.shift();
    render(r);
  }

  function start() {
    if (running) return;
    if (data.length === 0) resetSource();
    running = true;
    setConnected(true);
    $("startBtn").disabled = true;
    $("pauseBtn").disabled = false;
    log("Stream started (" + mode + ", " + SENSOR_TITLES[currentSensor()] + ")", "ok");
    tick();
    timer = setInterval(tick, INTERVAL);
  }

  function pause() {
    running = false;
    clearInterval(timer);
    setConnected(false);
    $("startBtn").disabled = false;
    $("pauseBtn").disabled = true;
    log("Stream paused", "");
  }

  /* ---- Render ---- */
  function render(r) {
    const unit = SENSORS[currentSensor()].unit;
    const values = data.map((d) => d.value);
    const min = Math.min(...values);
    const max = Math.max(...values);
    const mean = values.reduce((a, b) => a + b, 0) / values.length;
    const dp = currentSensor() === "voltage" ? 3 : 1;

    $("curVal").textContent = r.value.toFixed(dp);
    $("curUnit").textContent = unit;
    $("minVal").textContent = min.toFixed(dp);
    $("maxVal").textContent = max.toFixed(dp);
    $("meanVal").textContent = mean.toFixed(dp);

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

  $("sensorSel").addEventListener("change", () => {
    const s = currentSensor();
    $("chartTitle").textContent = SENSOR_TITLES[s];
    const defaults = { temp: [10, 30], light: [100, 900], voltage: [3.0, 3.5] };
    $("loThresh").value = defaults[s][0];
    $("hiThresh").value = defaults[s][1];
    const wasRunning = running;
    pause();
    resetSource();
    lastAlarm = "ok";
    if (wasRunning) start();
  });

  $("liveBtn").addEventListener("click", () => setMode("live"));
  $("replayBtn").addEventListener("click", () => setMode("replay"));

  function setMode(m) {
    if (mode === m) return;
    mode = m;
    $("liveBtn").classList.toggle("active", m === "live");
    $("replayBtn").classList.toggle("active", m === "replay");
    const wasRunning = running;
    pause();
    resetSource();
    log("Source: " + (m === "live" ? "live simulation" : "recorded replay"), "");
    if (wasRunning) start();
  }

  $("clearLog").addEventListener("click", emptyLog);

  /* ---- Init ---- */
  emptyLog();
  chart.draw([], { loThresh: 10, hiThresh: 30 });
})();
