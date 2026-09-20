import { defineConfig } from "@playwright/test";

export default defineConfig({
  testDir: "./tests/e2e",
  use: { baseURL: "http://localhost:4322" },
  webServer: {
    command: "npx --yes http-server -p 4322 -c-1 .",
    url: "http://localhost:4322",
    reuseExistingServer: !process.env.CI,
    timeout: 30000,
  },
});
