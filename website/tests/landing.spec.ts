import { expect, test } from "@playwright/test";

test("presents the game and keeps the page inside the viewport", async ({ page }, testInfo) => {
  await page.goto("./");

  await expect(page.getByRole("heading", { level: 1, name: "Shroomio" })).toBeVisible();
  await expect(page.getByText("Grow. Split. Dominate.")).toBeVisible();
  await expect(page.getByRole("link", { name: "Download for free" })).toHaveAttribute(
    "href",
    "/shroomio/download/",
  );
  const gameplay = page.locator("#gameplay");
  await expect(gameplay).toBeVisible();

  const hasHorizontalOverflow = await page.evaluate(
    () => document.documentElement.scrollWidth > document.documentElement.clientWidth,
  );
  expect(hasHorizontalOverflow).toBe(false);

  const gameplayTop = await gameplay.evaluate((element) => element.getBoundingClientRect().top);
  const viewportHeight = await page.evaluate(() => window.innerHeight);
  expect(gameplayTop).toBeLessThan(viewportHeight);

  await page.screenshot({ path: testInfo.outputPath("landing.png"), fullPage: true });
});

test("download navigation opens the platform page", async ({ page }) => {
  await page.goto("./");
  await page.getByRole("link", { name: "Download for free" }).click();

  await expect(page).toHaveURL(/\/shroomio\/download\/$/);
  await expect(page.getByRole("heading", { level: 1, name: "Download Shroomio" })).toBeVisible();
});

test("navigation reaches the gameplay section", async ({ page, isMobile }) => {
  await page.goto("./");

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

test("loads responsive gameplay screenshots without shifting the layout", async ({ page }) => {
  await page.goto("./");

  const gallery = page.getByRole("region", { name: "Gameplay screenshots" });
  const screenshots = gallery.getByRole("img");
  await gallery.scrollIntoViewIfNeeded();
  await expect(gallery).toBeVisible();
  await expect(screenshots).toHaveCount(3);

  for (const screenshot of await screenshots.all()) {
    await expect(screenshot).toBeVisible();
    await expect
      .poll(() => screenshot.evaluate((image: HTMLImageElement) => image.complete && image.naturalWidth > 0))
      .toBe(true);
    const box = await screenshot.boundingBox();
    expect(box?.width).toBeGreaterThan(200);
    expect(box?.height).toBeGreaterThan(100);
  }

  const hasHorizontalOverflow = await page.evaluate(
    () => document.documentElement.scrollWidth > document.documentElement.clientWidth,
  );
  expect(hasHorizontalOverflow).toBe(false);
});
