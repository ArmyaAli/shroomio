#include "unity.h"

#include "client/render_lod.h"

void setUp(void) {}

void tearDown(void) {}

static void test_render_lod_default_radius_is_full(void) {
  /* Spawn radius via ShroomMassToRadius(96) is 23.44 — well under MEDIUM. */
  TEST_ASSERT_EQUAL(SHROOM_PLAYER_LOD_FULL, ShroomPlayerRenderLodForRadius(0.0f));
  TEST_ASSERT_EQUAL(SHROOM_PLAYER_LOD_FULL, ShroomPlayerRenderLodForRadius(23.44f));
  TEST_ASSERT_EQUAL(SHROOM_PLAYER_LOD_FULL, ShroomPlayerRenderLodForRadius(69.99f));
}

static void test_render_lod_medium_tier_band(void) {
  TEST_ASSERT_EQUAL(SHROOM_PLAYER_LOD_MEDIUM, ShroomPlayerRenderLodForRadius(70.0f));
  TEST_ASSERT_EQUAL(SHROOM_PLAYER_LOD_MEDIUM, ShroomPlayerRenderLodForRadius(91.0f));
  TEST_ASSERT_EQUAL(SHROOM_PLAYER_LOD_MEDIUM, ShroomPlayerRenderLodForRadius(139.99f));
}

static void test_render_lod_minimal_tier_at_max_mass(void) {
  /* ShroomMassToRadius(SHROOM_MAX_PLAYER_MASS) = 10 + 0.14 * 1920 = 278.8. */
  TEST_ASSERT_EQUAL(SHROOM_PLAYER_LOD_MINIMAL, ShroomPlayerRenderLodForRadius(140.0f));
  TEST_ASSERT_EQUAL(SHROOM_PLAYER_LOD_MINIMAL, ShroomPlayerRenderLodForRadius(278.8f));
  TEST_ASSERT_EQUAL(SHROOM_PLAYER_LOD_MINIMAL, ShroomPlayerRenderLodForRadius(1e6f));
}

static void test_render_lod_negative_radius_is_full(void) {
  /* Defensive: sim never yields negative radius but the function must not
   * trip a tier meant for huge players on out-of-range input. */
  TEST_ASSERT_EQUAL(SHROOM_PLAYER_LOD_FULL, ShroomPlayerRenderLodForRadius(-1.0f));
}

static void test_render_lod_full_preserves_all_decorative_layers(void) {
  /* FULL must keep every legacy layer intact so default-spawn rendering is
   * byte-for-byte unchanged from before the LOD refactor. */
  TEST_ASSERT_EQUAL(1, ShroomPlayerRenderLodShouldDrawInnerHighlight(SHROOM_PLAYER_LOD_FULL));
  TEST_ASSERT_EQUAL(1, ShroomPlayerRenderLodShouldDrawPrimaryTopLight(SHROOM_PLAYER_LOD_FULL));
  TEST_ASSERT_EQUAL(1, ShroomPlayerRenderLodShouldDrawSecondaryHighlight(SHROOM_PLAYER_LOD_FULL));
  TEST_ASSERT_EQUAL(1, ShroomPlayerRenderLodShouldDrawSpeciesPattern(SHROOM_PLAYER_LOD_FULL));
  TEST_ASSERT_EQUAL(4, ShroomPlayerRenderLodTextShadowPasses(SHROOM_PLAYER_LOD_FULL));
}

static void test_render_lod_medium_drops_species_pattern_and_secondary(void) {
  /* MEDIUM drops the two heaviest per-player decorative passes (the secondary
   * top-light and the species pattern) and halves the text shadow. The local
   * inner cap highlight and primary top-light stay on so the cap keeps depth. */
  TEST_ASSERT_EQUAL(0, ShroomPlayerRenderLodShouldDrawInnerHighlight(SHROOM_PLAYER_LOD_MEDIUM));
  TEST_ASSERT_EQUAL(1, ShroomPlayerRenderLodShouldDrawPrimaryTopLight(SHROOM_PLAYER_LOD_MEDIUM));
  TEST_ASSERT_EQUAL(0, ShroomPlayerRenderLodShouldDrawSecondaryHighlight(SHROOM_PLAYER_LOD_MEDIUM));
  TEST_ASSERT_EQUAL(0, ShroomPlayerRenderLodShouldDrawSpeciesPattern(SHROOM_PLAYER_LOD_MEDIUM));
  TEST_ASSERT_EQUAL(2, ShroomPlayerRenderLodTextShadowPasses(SHROOM_PLAYER_LOD_MEDIUM));
}

static void test_render_lod_minimal_drops_all_decorative_layers(void) {
  /* MINIMAL: bounded decorative work. Every decorative predicate is off and
   * the text shadow collapses to a single pass — this is the acceptance
   * criterion for issue #375 ("high-mass rendering uses bounded decorative
   * work"). Only the three base cap fills (ground shadow, cap underside, main
   * cap) remain; primary top-light, inner highlight, secondary highlight and
   * species pattern all drop so high-mass players pay bounded alpha-blended
   * fill cost regardless of mass. */
  TEST_ASSERT_EQUAL(0, ShroomPlayerRenderLodShouldDrawInnerHighlight(SHROOM_PLAYER_LOD_MINIMAL));
  TEST_ASSERT_EQUAL(0, ShroomPlayerRenderLodShouldDrawPrimaryTopLight(SHROOM_PLAYER_LOD_MINIMAL));
  TEST_ASSERT_EQUAL(0,
                    ShroomPlayerRenderLodShouldDrawSecondaryHighlight(SHROOM_PLAYER_LOD_MINIMAL));
  TEST_ASSERT_EQUAL(0, ShroomPlayerRenderLodShouldDrawSpeciesPattern(SHROOM_PLAYER_LOD_MINIMAL));
  TEST_ASSERT_EQUAL(1, ShroomPlayerRenderLodTextShadowPasses(SHROOM_PLAYER_LOD_MINIMAL));
}

static void test_render_lod_threshold_cells_are_stable_boundaries(void) {
  /* Snapshot the breakpoint constants — changing these silently shifts the
   * FPS-vs-mass cliff and should be a deliberate decision. */
  TEST_ASSERT_EQUAL_FLOAT(70.0f, SHROOM_PLAYER_LOD_MEDIUM_RADIUS);
  TEST_ASSERT_EQUAL_FLOAT(140.0f, SHROOM_PLAYER_LOD_MINIMAL_RADIUS);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_render_lod_default_radius_is_full);
  RUN_TEST(test_render_lod_medium_tier_band);
  RUN_TEST(test_render_lod_minimal_tier_at_max_mass);
  RUN_TEST(test_render_lod_negative_radius_is_full);
  RUN_TEST(test_render_lod_full_preserves_all_decorative_layers);
  RUN_TEST(test_render_lod_medium_drops_species_pattern_and_secondary);
  RUN_TEST(test_render_lod_minimal_drops_all_decorative_layers);
  RUN_TEST(test_render_lod_threshold_cells_are_stable_boundaries);
  return UNITY_END();
}