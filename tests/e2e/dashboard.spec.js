import { test, expect } from "@playwright/test";
import AxeBuilder from "@axe-core/playwright";

test("dashboard has no serious WCAG A/AA violations", async ({ page }) => {
  await page.goto("/");
  const results = await new AxeBuilder({ page })
    .withTags(["wcag2a", "wcag2aa"])
    .analyze();
  expect(results.violations).toEqual([]);
});

test("streaming updates the current reading and event log", async ({ page }) => {
  await page.goto("/");
  await expect(page.locator("#curVal")).toHaveText("—");

  await page.getByRole("button", { name: "Start" }).click();

  // a reading should appear and the log should record the stream start
  await expect(page.locator("#curVal")).not.toHaveText("—", { timeout: 3000 });
  await expect(page.locator("#log")).toContainText("Stream started");
  await expect(page.locator(".conn")).toHaveClass(/on/);
});
