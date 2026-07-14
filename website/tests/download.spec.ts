import { expect, test } from "@playwright/test";

const latestRelease = {
  tag_name: "v0.4.0",
  published_at: "2026-07-14T12:00:00Z",
  html_url: "https://github.com/ArmyaAli/shroomio/releases/tag/v0.4.0",
  assets: [
    ["shroomio-0.4.0-windows-x64.zip", 3000000],
    ["shroomio-0.4.0-linux-x64.tar.gz", 2500000],
    ["shroomio-0.4.0-macos-x64.tar.gz", 2400000],
    ["SHA256SUMS-v0.4.0.txt", 400],
  ].map(([name, size]) => ({
    name,
    size,
    browser_download_url: `https://github.com/ArmyaAli/shroomio/releases/download/v0.4.0/${name}`,
  })),
};

test.beforeEach(async ({ page }) => {
  await page.route("https://api.github.com/repos/ArmyaAli/shroomio/releases/latest", (route) =>
    route.fulfill({ json: latestRelease }),
  );
});

test("shows direct downloads, release metadata, and verification links", async ({ page }, testInfo) => {
  await page.goto("/download/");

  await expect(page.getByRole("heading", { level: 1, name: "Download Shroomio" })).toBeVisible();
  await expect(page.locator("[data-release-version]").first()).toHaveText("0.4.0");
  await expect(page.locator("[data-release-date]")).toHaveText("July 14, 2026");

  for (const platform of ["Windows", "Linux", "macOS"]) {
    const card = page.locator("[data-platform-card]", { has: page.getByRole("heading", { name: platform }) });
    await expect(card).toBeVisible();
    await expect(card.getByRole("link", { name: `Download ${platform}` })).toHaveAttribute(
      "href",
      /github\.com\/ArmyaAli\/shroomio\/releases\/download\/v0\.4\.0/,
    );
  }

  await expect(page.getByRole("link", { name: "Download checksums" })).toHaveAttribute(
    "href",
    /SHA256SUMS-v0\.4\.0\.txt$/,
  );
  await expect(page.getByRole("link", { name: "Release notes" })).toHaveAttribute(
    "href",
    /releases\/tag\/v0\.4\.0$/,
  );

  expect(await page.evaluate(
    () => document.documentElement.scrollWidth > document.documentElement.clientWidth,
  )).toBe(false);
  await page.screenshot({ path: testInfo.outputPath("downloads.png"), fullPage: true });
});

test("keeps the checked-in stable release when GitHub is unavailable", async ({ page }) => {
  await page.unroute("https://api.github.com/repos/ArmyaAli/shroomio/releases/latest");
  await page.route("https://api.github.com/repos/ArmyaAli/shroomio/releases/latest", (route) =>
    route.fulfill({ status: 503, body: "unavailable" }),
  );
  await page.goto("/download/");

  await expect(page.locator("[data-release-version]").first()).toHaveText("0.3.0");
  await expect(page.getByRole("link", { name: "Download Windows" })).toHaveAttribute(
    "href",
    /releases\/download\/v0\.3\.0\/shroomio-0\.3\.0-windows-x64\.zip$/,
  );
});
