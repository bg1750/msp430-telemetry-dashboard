/* Alarm evaluation — pure, so it can be unit-tested without the DOM.
   Returns the state for a reading against low/high thresholds. */

export const NEAR_LIMIT_FRACTION = 0.1; // within 10% of a limit → "warn"

// value, lo, hi -> { state: "ok"|"warn"|"alarm", label }
export function evaluateAlarm(value, lo, hi) {
  const band = hi - lo;
  const margin = band * NEAR_LIMIT_FRACTION;

  if (value > hi) return { state: "alarm", label: "HIGH alarm" };
  if (value < lo) return { state: "alarm", label: "LOW alarm" };
  if (value > hi - margin || value < lo + margin) return { state: "warn", label: "Near limit" };
  return { state: "ok", label: "Nominal" };
}

if (typeof window !== "undefined") window.Alarm = { evaluateAlarm, NEAR_LIMIT_FRACTION };
