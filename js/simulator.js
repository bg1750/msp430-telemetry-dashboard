/* Telemetry source — stands in for the real firmware→UART→Python→WebSocket pipeline.
   Emits framed readings identical in shape to what a host service would forward. */

export const SENSORS = {
  temp:    { unit: "°C",  base: 22,   noise: 0.6,  drift: 0.04,  min: -5,  max: 60  },
  light:   { unit: "lux", base: 420,  noise: 30,   drift: 4,     min: 0,   max: 1200 },
  voltage: { unit: "V",   base: 3.30, noise: 0.02, drift: 0.005, min: 2.6, max: 3.6 },
};

// A single reading, framed the way the device protocol documents it.
export function frame(sensor, value, seq) {
  return {
    seq,
    sensor,
    unit: SENSORS[sensor].unit,
    value: Number(value.toFixed(sensor === "voltage" ? 3 : 2)),
    t: Date.now(),
  };
}

export function Simulator(sensorKey) {
  const cfg = SENSORS[sensorKey];
  let v = cfg.base;
  let seq = 0;
  let dir = 1;

  return {
    unit: cfg.unit,
    next() {
      seq++;
      // slow random walk with occasional excursions toward thresholds
      if (Math.random() < 0.04) dir *= -1;
      v += dir * cfg.drift + (Math.random() - 0.5) * cfg.noise;
      // rare spike to exercise the alarm path
      if (Math.random() < 0.02) v += (Math.random() - 0.5) * cfg.noise * 8;
      v = Math.max(cfg.min, Math.min(cfg.max, v));
      return frame(sensorKey, v, seq);
    },
  };
}

// Pre-recorded run for Replay mode — a fixed-length buffer the UI loops over.
export function makeReplay(sensorKey, n) {
  const sim = Simulator(sensorKey);
  const out = [];
  for (let i = 0; i < n; i++) out.push(sim.next());
  return out;
}

// Back-compat for a plain <script> include, if ever used that way.
if (typeof window !== "undefined") {
  window.Telemetry = { Simulator, makeReplay, SENSORS, frame };
}
