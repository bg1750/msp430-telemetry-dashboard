import { test, expect } from "@playwright/test";
import AxeBuilder from "@axe-core/playwright";

test("dashboard has no serious WCAG A/AA violations", async ({ page }) => {
  await page.goto("/");
  const results = await new AxeBuilder({ page })
    .withTags(["wcag2a", "wcag2aa"])
    .analyze();
  expect(results.violations).toEqual([]);
});

test("Start initiates a live connection to the host", async ({ page }) => {
  await page.goto("/");
  await expect(page.locator("#curVal")).toHaveText("—");

  await page.getByRole("button", { name: "Start" }).click();

  // The dashboard's only source is the live host, so Start opens the WebSocket.
  // (No host runs in CI, so we assert the connection *attempt* and UI state,
  // not that data arrives — that needs the board + host.py.)
  await expect(page.locator("#log")).toContainText("Connecting to host");
  await expect(page.getByRole("button", { name: "Start" })).toBeDisabled();
  await expect(page.getByRole("button", { name: "Pause" })).toBeEnabled();
});
