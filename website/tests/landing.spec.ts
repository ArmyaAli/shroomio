import { expect, test } from "@playwright/test";

test("presents the game and keeps the page inside the viewport", async ({ page }, testInfo) => {
  await page.goto("/");

  await expect(page.getByRole("heading", { level: 1, name: "Shroomio" })).toBeVisible();
  await expect(page.getByText("Grow. Split. Dominate.")).toBeVisible();
  await expect(page.getByRole("link", { name: "Download for free" })).toHaveAttribute(
    "href",
    "https://github.com/ArmyaAli/shroomio/releases",
  );
  await expect(page.locator("#gameplay")).toBeVisible();

  const hasHorizontalOverflow = await page.evaluate(
    () => document.documentElement.scrollWidth > document.documentElement.clientWidth,
  );
  expect(hasHorizontalOverflow).toBe(false);

  await page.screenshot({ path: testInfo.outputPath("landing.png"), fullPage: true });
});

test("navigation reaches the gameplay section", async ({ page, isMobile }) => {
  await page.goto("/");

  if (isMobile) {
    await page.getByLabel("Open navigation").click();
    await expect(page.getByRole("navigation", { name: "Mobile navigation" })).toBeVisible();
    await page
      .getByRole("navigation", { name: "Mobile navigation" })
      .getByRole("link", { name: "Gameplay" })
      .click();
  } else {
    await page
      .getByRole("navigation", { name: "Primary navigation" })
      .getByRole("link", { name: "Gameplay" })
      .click();
  }

  await expect(page).toHaveURL(/#gameplay$/);
  await expect(page.getByRole("heading", { name: "Small spore. Big ambition." })).toBeInViewport();
});
