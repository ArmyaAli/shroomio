#include "unity.h"

#include "client/screens/screen_background.h"

#include "raylib.h"

#include <math.h>

static float MaxAbs3(float a, float b, float c) {
  const float max_ab = fmaxf(fabsf(a), fabsf(b));
  return fmaxf(max_ab, fabsf(c));
}

int GetScreenWidth(void) { return 1280; }
int GetScreenHeight(void) { return 720; }
double GetTime(void) { return 0.0; }
void DrawLine(int start_pos_x, int start_pos_y, int end_pos_x, int end_pos_y, Color color) {
  (void)start_pos_x;
  (void)start_pos_y;
  (void)end_pos_x;
  (void)end_pos_y;
  (void)color;
}
void DrawRectangle(int pos_x, int pos_y, int width, int height, Color color) {
  (void)pos_x;
  (void)pos_y;
  (void)width;
  (void)height;
  (void)color;
}
void DrawEllipse(int center_x, int center_y, float radius_h, float radius_v, Color color) {
  (void)center_x;
  (void)center_y;
  (void)radius_h;
  (void)radius_v;
  (void)color;
}
void DrawCircle(int center_x, int center_y, float radius, Color color) {
  (void)center_x;
  (void)center_y;
  (void)radius;
  (void)color;
}
void DrawCircleV(Vector2 center, float radius, Color color) {
  (void)center;
  (void)radius;
  (void)color;
}

void setUp(void) { ShroomScreenResetFungalBackground(); }

void tearDown(void) {}

void test_fungal_background_animates_mushroom_pose_when_enabled(void) {
  const ShroomFungalBackgroundDebugState before =
      ShroomScreenGetFungalBackgroundDebugState(0, true, 1280, 720);

  for (int frame = 0; frame < 10; ++frame) {
    ShroomScreenUpdateFungalBackground(0.05f, true);
  }

  const ShroomFungalBackgroundDebugState after =
      ShroomScreenGetFungalBackgroundDebugState(0, true, 1280, 720);

  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, after.global_time);
  TEST_ASSERT_GREATER_THAN_FLOAT(20.0f, MaxAbs3(after.mushroom_x - before.mushroom_x,
                                                after.mushroom_y - before.mushroom_y,
                                                after.mushroom_sway - before.mushroom_sway));
}

void test_fungal_background_stays_still_when_animation_disabled(void) {
  const ShroomFungalBackgroundDebugState before =
      ShroomScreenGetFungalBackgroundDebugState(0, false, 1280, 720);

  for (int frame = 0; frame < 10; ++frame) {
    ShroomScreenUpdateFungalBackground(0.05f, false);
  }

  const ShroomFungalBackgroundDebugState after =
      ShroomScreenGetFungalBackgroundDebugState(0, false, 1280, 720);

  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, after.global_time);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, before.mushroom_x, after.mushroom_x);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, before.mushroom_y, after.mushroom_y);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, before.mushroom_sway, after.mushroom_sway);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_fungal_background_animates_mushroom_pose_when_enabled);
  RUN_TEST(test_fungal_background_stays_still_when_animation_disabled);
  return UNITY_END();
}
