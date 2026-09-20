import { describe, it, expect } from "vitest";
import { evaluateAlarm } from "../js/alarm.js";

describe("evaluateAlarm", () => {
  const lo = 10, hi = 30; // band 20, margin 2

  it("is nominal comfortably inside the band", () => {
    expect(evaluateAlarm(20, lo, hi)).toEqual({ state: "ok", label: "Nominal" });
  });

  it("raises a HIGH alarm above the high threshold", () => {
    expect(evaluateAlarm(31, lo, hi)).toEqual({ state: "alarm", label: "HIGH alarm" });
  });

  it("raises a LOW alarm below the low threshold", () => {
    expect(evaluateAlarm(9, lo, hi)).toEqual({ state: "alarm", label: "LOW alarm" });
  });

  it("warns when near the high limit", () => {
    expect(evaluateAlarm(29, lo, hi)).toEqual({ state: "warn", label: "Near limit" });
  });

  it("warns when near the low limit", () => {
    expect(evaluateAlarm(11, lo, hi)).toEqual({ state: "warn", label: "Near limit" });
  });

  it("treats the exact threshold as in-band (not yet alarming)", () => {
    // 30 is not > 30, so it's a near-limit warning rather than an alarm
    expect(evaluateAlarm(30, lo, hi).state).toBe("warn");
    expect(evaluateAlarm(10, lo, hi).state).toBe("warn");
  });
});
