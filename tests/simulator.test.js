import { describe, it, expect } from "vitest";
import { Simulator, makeReplay, frame, SENSORS } from "../js/simulator.js";

describe("frame", () => {
  it("shapes a reading the way the device protocol documents it", () => {
    const f = frame("temp", 22.456, 7);
    expect(f).toMatchObject({ seq: 7, sensor: "temp", unit: "°C", value: 22.46 });
    expect(typeof f.t).toBe("number");
  });

  it("uses 3 decimal places for voltage", () => {
    expect(frame("voltage", 3.30159, 1).value).toBe(3.302);
  });
});

describe("Simulator", () => {
  it("increments the sequence number each reading", () => {
    const sim = Simulator("temp");
    expect(sim.next().seq).toBe(1);
    expect(sim.next().seq).toBe(2);
  });

  it("clamps readings to the sensor's physical range", () => {
    const { min, max } = SENSORS.voltage;
    const sim = Simulator("voltage");
    for (let i = 0; i < 500; i++) {
      const v = sim.next().value;
      expect(v).toBeGreaterThanOrEqual(min);
      expect(v).toBeLessThanOrEqual(max);
    }
  });
});

describe("makeReplay", () => {
  it("returns a buffer of the requested length", () => {
    expect(makeReplay("light", 60)).toHaveLength(60);
  });

  it("produces monotonically increasing sequence numbers", () => {
    const buf = makeReplay("temp", 10);
    const seqs = buf.map((r) => r.seq);
    expect(seqs).toEqual([...seqs].sort((a, b) => a - b));
  });
});
