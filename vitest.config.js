import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    // Unit tests only; Playwright e2e specs (tests/e2e/*.spec.js) run separately.
    include: ["tests/**/*.test.js"],
  },
});
