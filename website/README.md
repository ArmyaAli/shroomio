# Shroomio Website

The Astro site is published at <https://armyaali.github.io/shroomio/>. It is a static build with the
repository base path `/shroomio`, so internal links and generated assets work on GitHub Pages.

## Local Development

Requires Node.js 22.12 or newer.

```sh
cd website
npm ci
npm run dev
```

Open `http://localhost:4321/shroomio/`. Use `npm run check` for Astro diagnostics,
`npm run test:unit` for release-data tests, and `npm run test:e2e` for responsive browser coverage.

## Production Build

```sh
cd website
npm run test:build
npm run preview
```

The command writes the deployable site to `website/dist/` and verifies its routes, base-aware links,
bundled JavaScript, and optimized WebP images. Preview the result at
`http://localhost:4321/shroomio/`.

## Deployment

`.github/workflows/deploy-website.yml` builds and deploys the site whenever website or marketing
assets change on `main`. It can also be started manually from **Actions > Deploy Website > Run
workflow**. The build job validates the project and uploads `website/dist/`; the deploy job publishes
that artifact to the protected `github-pages` environment. Check the workflow run and the Pages
deployment status after a release. Do not commit `website/dist/`.
