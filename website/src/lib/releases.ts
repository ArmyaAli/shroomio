export interface GitHubReleaseAsset {
  name: string;
  browser_download_url: string;
  size: number;
}

export interface GitHubRelease {
  tag_name: string;
  published_at: string;
  html_url: string;
  assets: GitHubReleaseAsset[];
}

export interface DownloadRelease {
  version: string;
  publishedLabel: string;
  releaseUrl: string;
  checksumUrl: string;
  platforms: Record<"windows" | "linux" | "macos", { url: string; sizeLabel: string }>;
}

const PLATFORM_SUFFIXES = {
  windows: "-windows-x64.zip",
  linux: "-linux-x64.tar.gz",
  macos: "-macos-x64.tar.gz",
} as const;

export const FALLBACK_GITHUB_RELEASE: GitHubRelease = {
  tag_name: "v0.3.0",
  published_at: "2026-07-12T15:33:16Z",
  html_url: "https://github.com/ArmyaAli/shroomio/releases/tag/v0.3.0",
  assets: [
    {
      name: "shroomio-0.3.0-windows-x64.zip",
      browser_download_url:
        "https://github.com/ArmyaAli/shroomio/releases/download/v0.3.0/shroomio-0.3.0-windows-x64.zip",
      size: 2670702,
    },
    {
      name: "shroomio-0.3.0-linux-x64.tar.gz",
      browser_download_url:
        "https://github.com/ArmyaAli/shroomio/releases/download/v0.3.0/shroomio-0.3.0-linux-x64.tar.gz",
      size: 2262531,
    },
    {
      name: "shroomio-0.3.0-macos-x64.tar.gz",
      browser_download_url:
        "https://github.com/ArmyaAli/shroomio/releases/download/v0.3.0/shroomio-0.3.0-macos-x64.tar.gz",
      size: 2078042,
    },
    {
      name: "SHA256SUMS-v0.3.0.txt",
      browser_download_url:
        "https://github.com/ArmyaAli/shroomio/releases/download/v0.3.0/SHA256SUMS-v0.3.0.txt",
      size: 380,
    },
  ],
};

function IsAsset(value: unknown): value is GitHubReleaseAsset {
  if ((typeof value !== "object") || (value === null)) {
    return false;
  }
  const asset = value as Record<string, unknown>;
  return (typeof asset.name === "string") &&
    (typeof asset.browser_download_url === "string") &&
    (typeof asset.size === "number") &&
    asset.browser_download_url.startsWith("https://github.com/ArmyaAli/shroomio/releases/download/");
}

export function ParseGitHubRelease(value: unknown): GitHubRelease | null {
  if ((typeof value !== "object") || (value === null)) {
    return null;
  }
  const release = value as Record<string, unknown>;
  if ((typeof release.tag_name !== "string") || !/^v\d+\.\d+\.\d+$/.test(release.tag_name) ||
      (typeof release.published_at !== "string") || Number.isNaN(Date.parse(release.published_at)) ||
      (typeof release.html_url !== "string") || !release.html_url.startsWith("https://github.com/ArmyaAli/shroomio/releases/") ||
      !Array.isArray(release.assets) || !release.assets.every(IsAsset)) {
    return null;
  }
  return release as unknown as GitHubRelease;
}

export function FormatAssetSize(bytes: number): string {
  if (!Number.isFinite(bytes) || (bytes <= 0)) {
    return "Archive";
  }
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

export function ResolveDownloadRelease(release: GitHubRelease): DownloadRelease | null {
  const assets = Object.fromEntries(
    Object.entries(PLATFORM_SUFFIXES).map(([platform, suffix]) => [
      platform,
      release.assets.find((asset) => asset.name.endsWith(suffix)),
    ]),
  ) as Record<keyof typeof PLATFORM_SUFFIXES, GitHubReleaseAsset | undefined>;
  const checksum = release.assets.find((asset) => asset.name.startsWith("SHA256SUMS-v"));
  if (!assets.windows || !assets.linux || !assets.macos || !checksum) {
    return null;
  }

  return {
    version: release.tag_name.slice(1),
    publishedLabel: new Intl.DateTimeFormat("en-US", {
      month: "long",
      day: "numeric",
      year: "numeric",
      timeZone: "UTC",
    }).format(new Date(release.published_at)),
    releaseUrl: release.html_url,
    checksumUrl: checksum.browser_download_url,
    platforms: {
      windows: { url: assets.windows.browser_download_url, sizeLabel: FormatAssetSize(assets.windows.size) },
      linux: { url: assets.linux.browser_download_url, sizeLabel: FormatAssetSize(assets.linux.size) },
      macos: { url: assets.macos.browser_download_url, sizeLabel: FormatAssetSize(assets.macos.size) },
    },
  };
}
