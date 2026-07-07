#include "render_lod.h"

ShroomPlayerRenderLodTier ShroomPlayerRenderLodForRadius(float radius) {
  if (radius >= SHROOM_PLAYER_LOD_MINIMAL_RADIUS) {
    return SHROOM_PLAYER_LOD_MINIMAL;
  }
  if (radius >= SHROOM_PLAYER_LOD_MEDIUM_RADIUS) {
    return SHROOM_PLAYER_LOD_MEDIUM;
  }
  return SHROOM_PLAYER_LOD_FULL;
}

int ShroomPlayerRenderLodShouldDrawInnerHighlight(ShroomPlayerRenderLodTier tier) {
  return tier == SHROOM_PLAYER_LOD_FULL;
}

int ShroomPlayerRenderLodShouldDrawPrimaryTopLight(ShroomPlayerRenderLodTier tier) {
  return tier != SHROOM_PLAYER_LOD_MINIMAL;
}

int ShroomPlayerRenderLodShouldDrawSecondaryHighlight(ShroomPlayerRenderLodTier tier) {
  return tier == SHROOM_PLAYER_LOD_FULL;
}

int ShroomPlayerRenderLodShouldDrawSpeciesPattern(ShroomPlayerRenderLodTier tier) {
  return tier == SHROOM_PLAYER_LOD_FULL;
}

int ShroomPlayerRenderLodTextShadowPasses(ShroomPlayerRenderLodTier tier) {
  switch (tier) {
  case SHROOM_PLAYER_LOD_MINIMAL:
    return 1;
  case SHROOM_PLAYER_LOD_MEDIUM:
    return 2;
  case SHROOM_PLAYER_LOD_FULL:
  default:
    return 4;
  }
}