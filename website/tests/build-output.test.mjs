import assert from "node:assert/strict";
import { readFile, readdir, stat } from "node:fs/promises";
import path from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

const websiteDirectory = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const distDirectory = path.join(websiteDirectory, "dist");

test("build emits base-aware static routes and optimized assets", async () => {
  const [landing, download, assetNames] = await Promise.all([
    readFile(path.join(distDirectory, "index.html"), "utf8"),
    readFile(path.join(distDirectory, "download", "index.html"), "utf8"),
    readdir(path.join(distDirectory, "_astro")),
  ]);

  assert.match(landing, /href="\/shroomio\/download\/"/);
  assert.match(landing, /\/shroomio\/_astro\//);
  assert.match(download, /href="\/shroomio\/"/);
  assert.doesNotMatch(`${landing}\n${download}`, /(?:src|href)="\/_astro\//);

  const webpAssets = assetNames.filter((name) => name.endsWith(".webp"));
  assert.ok(webpAssets.length >= 3, "expected responsive WebP image output");
  assert.ok(assetNames.some((name) => name.endsWith(".css")), "expected minified CSS output");
  assert.match(download, /<script type="module">[^\n]+<\/script>/, "expected minified client JavaScript");

  for (const name of assetNames) {
    const asset = await stat(path.join(distDirectory, "_astro", name));
    assert.ok(asset.size > 0, `${name} must not be empty`);
  }
});
