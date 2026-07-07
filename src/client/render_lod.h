#ifndef SHROOM_CLIENT_RENDER_LOD_H
#define SHROOM_CLIENT_RENDER_LOD_H

#include <stdint.h>

/* Per-player level-of-detail tier used by the renderer to cap decorative work
 * as players grow. The mass-to-radius formula `ShroomMassToRadius` keeps large
 * players bounded by `SHROOM_MAX_PLAYER_MASS`, but the alpha-blended fill cost
 * scales with area, so decorative layers (highlights, species patterns, text
 * shadow passes) must collapse as radius grows to keep high-mass rendering
 * bounded. See issue #375. */
typedef enum ShroomPlayerRenderLodTier {
  SHROOM_PLAYER_LOD_FULL = 0,
  SHROOM_PLAYER_LOD_MEDIUM,
  SHROOM_PLAYER_LOD_MINIMAL,
} ShroomPlayerRenderLodTier;

/* Radius breakpoints. MEDIUM drops the species pattern and the secondary
 * top-light highlight and halves the text shadow passes; MINIMAL also drops
 * the primary top-light highlight, the local inner highlight, and reduces
 * the text shadow to a single pass. Thresholds were chosen so default-spawn
 * players (radius ~23 at mass 96) and modestly-grown players stay on FULL
 * (the rendered look is unchanged), while high-mass players past the decay
 * threshold (radius ~91 at mass 576) fall through MEDIUM and MINIMAL tiers. */
#define SHROOM_PLAYER_LOD_MEDIUM_RADIUS 70.0f
#define SHROOM_PLAYER_LOD_MINIMAL_RADIUS 140.0f

ShroomPlayerRenderLodTier ShroomPlayerRenderLodForRadius(float radius);

/* Per-decorative-layer predicates. DrawPlayers gates each decorative layer
 * through these so the gating policy lives in one testable place. */
int ShroomPlayerRenderLodShouldDrawInnerHighlight(ShroomPlayerRenderLodTier tier);
int ShroomPlayerRenderLodShouldDrawPrimaryTopLight(ShroomPlayerRenderLodTier tier);
int ShroomPlayerRenderLodShouldDrawSecondaryHighlight(ShroomPlayerRenderLodTier tier);
int ShroomPlayerRenderLodShouldDrawSpeciesPattern(ShroomPlayerRenderLodTier tier);

/* Number of offset shadow DrawText passes per tier. FULL keeps the legacy
 * 4-pass shadow for crisp text; MEDIUM collapses to 2; MINIMAL uses a single
 * pass so a high-mass player pays at most one text-shadow fill instead of 4. */
int ShroomPlayerRenderLodTextShadowPasses(ShroomPlayerRenderLodTier tier);

#endif