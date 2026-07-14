import assert from "node:assert/strict";
import test from "node:test";

import {
  FALLBACK_GITHUB_RELEASE,
  FormatAssetSize,
  ParseGitHubRelease,
  ResolveDownloadRelease,
} from "../src/lib/releases.ts";

test("maps a complete GitHub release to direct platform downloads", () => {
  const release = ResolveDownloadRelease(FALLBACK_GITHUB_RELEASE);

  assert.ok(release);
  assert.equal(release.version, "0.3.0");
  assert.equal(release.publishedLabel, "July 12, 2026");
  assert.match(release.platforms.windows.url, /shroomio-0\.3\.0-windows-x64\.zip$/);
  assert.match(release.platforms.linux.url, /shroomio-0\.3\.0-linux-x64\.tar\.gz$/);
  assert.match(release.platforms.macos.url, /shroomio-0\.3\.0-macos-x64\.tar\.gz$/);
  assert.match(release.checksumUrl, /SHA256SUMS-v0\.3\.0\.txt$/);
});

test("rejects incomplete, malformed, and off-repository release data", () => {
  assert.equal(ParseGitHubRelease(null), null);
  assert.equal(ParseGitHubRelease({ ...FALLBACK_GITHUB_RELEASE, tag_name: "latest" }), null);
  assert.equal(
    ParseGitHubRelease({
      ...FALLBACK_GITHUB_RELEASE,
      assets: [{
        ...FALLBACK_GITHUB_RELEASE.assets[0],
        browser_download_url: "https://example.com/untrusted.zip",
      }],
    }),
    null,
  );
  assert.equal(
    ResolveDownloadRelease({ ...FALLBACK_GITHUB_RELEASE, assets: FALLBACK_GITHUB_RELEASE.assets.slice(0, 2) }),
    null,
  );
});

test("formats archive sizes with a deterministic fallback", () => {
  assert.equal(FormatAssetSize(2670702), "2.5 MB");
  assert.equal(FormatAssetSize(0), "Archive");
  assert.equal(FormatAssetSize(Number.NaN), "Archive");
});
