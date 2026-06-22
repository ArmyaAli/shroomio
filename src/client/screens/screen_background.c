#include "screen_background.h"

#include "raylib.h"

#include <math.h>
#include <stdint.h>

#define SHROOM_MENU_BACKGROUND_SPORE_COUNT 50
#define SHROOM_MENU_BACKGROUND_MUSHROOM_COUNT 8
#define SHROOM_MENU_BACKGROUND_PARTICLE_COUNT 100
#define SHROOM_MENU_BACKGROUND_MUSHROOM_DRIFT_RATIO 0.08f
#define SHROOM_MENU_BACKGROUND_MUSHROOM_BOB_RATIO 0.03f

typedef struct MenuBackgroundSpore {
  float x_ratio;
  float y_ratio;
  float speed;
  float size;
  float alpha;
  float phase;
} MenuBackgroundSpore;

typedef struct MenuBackgroundMushroom {
  float x_ratio;
  float y_ratio;
  float scale;
  float sway_phase;
  float sway_speed;
  int type;
} MenuBackgroundMushroom;

typedef struct MenuBackgroundParticle {
  float x_ratio;
  float y_ratio;
  float vx;
  float vy;
  float life;
  float max_life;
  float size;
  Color color;
} MenuBackgroundParticle;

typedef struct FungalMushroomPose {
  float global_time;
  float mushroom_x;
  float mushroom_y;
  float mushroom_sway;
} FungalMushroomPose;

static MenuBackgroundSpore g_spores[SHROOM_MENU_BACKGROUND_SPORE_COUNT];
static MenuBackgroundMushroom g_mushrooms[SHROOM_MENU_BACKGROUND_MUSHROOM_COUNT];
static MenuBackgroundParticle g_particles[SHROOM_MENU_BACKGROUND_PARTICLE_COUNT];
static float g_global_time = 0.0f;
static float g_spawn_accumulator = 0.0f;
static bool g_background_initialized = false;

static float WrappedUnit(float value) {
  value = fmodf(value, 1.0f);
  if (value < 0.0f) {
    value += 1.0f;
  }
  return value;
}

static float HashUnit(int index, int salt) {
  return WrappedUnit(sinf((float)(index * 37 + salt * 101)) * 43758.547f);
}

static void SpawnParticle(float x_ratio, float y_ratio) {
  for (int index = 0; index < SHROOM_MENU_BACKGROUND_PARTICLE_COUNT; ++index) {
    MenuBackgroundParticle* particle = &g_particles[index];
    if (particle->life <= 0.0f) {
      const float seed = HashUnit(index, (int)(g_global_time * 17.0f));
      particle->x_ratio = x_ratio;
      particle->y_ratio = y_ratio;
      particle->vx = (seed - 0.5f) * 0.06f;
      particle->vy = -(0.05f + HashUnit(index, 19) * 0.08f);
      particle->life = 1.6f + HashUnit(index, 23) * 1.4f;
      particle->max_life = particle->life;
      particle->size = 1.0f + HashUnit(index, 29) * 3.0f;
      particle->color = (Color){200, 150, 255, 200};
      break;
    }
  }
}

void ShroomScreenResetFungalBackground(void) {
  for (int index = 0; index < SHROOM_MENU_BACKGROUND_SPORE_COUNT; ++index) {
    MenuBackgroundSpore* spore = &g_spores[index];
    spore->x_ratio = HashUnit(index, 1);
    spore->y_ratio = HashUnit(index, 2);
    spore->speed = 0.035f + HashUnit(index, 3) * 0.065f;
    spore->size = 2.0f + HashUnit(index, 4) * 4.0f;
    spore->alpha = 0.28f + HashUnit(index, 5) * 0.52f;
    spore->phase = HashUnit(index, 6) * 10.0f;
  }

  for (int index = 0; index < SHROOM_MENU_BACKGROUND_MUSHROOM_COUNT; ++index) {
    MenuBackgroundMushroom* mushroom = &g_mushrooms[index];
    mushroom->x_ratio = WrappedUnit(0.11f + (float)index * 0.137f);
    mushroom->y_ratio = 0.70f + WrappedUnit(0.19f + (float)index * 0.21f) * 0.28f;
    mushroom->scale = 0.55f + WrappedUnit(0.31f + (float)index * 0.17f) * 0.95f;
    mushroom->sway_phase = HashUnit(index, 7) * 10.0f;
    mushroom->sway_speed = 0.55f + HashUnit(index, 8) * 0.65f;
    mushroom->type = index % 3;
  }

  for (int index = 0; index < SHROOM_MENU_BACKGROUND_PARTICLE_COUNT; ++index) {
    g_particles[index].life = 0.0f;
  }

  g_global_time = 0.0f;
  g_spawn_accumulator = 0.0f;
  g_background_initialized = true;
}

void ShroomScreenUpdateFungalBackground(float delta_time, bool animate) {
  if (!g_background_initialized) {
    ShroomScreenResetFungalBackground();
  }
  if (!animate) {
    return;
  }

  delta_time = fminf(fmaxf(delta_time, 0.0f), 0.05f);
  g_global_time += delta_time;

  for (int index = 0; index < SHROOM_MENU_BACKGROUND_SPORE_COUNT; ++index) {
    MenuBackgroundSpore* spore = &g_spores[index];
    spore->y_ratio -= spore->speed * delta_time;
    spore->x_ratio += sinf(g_global_time + spore->phase) * 0.018f * delta_time;
    if (spore->y_ratio < -0.02f) {
      spore->y_ratio = 1.02f;
      spore->x_ratio = HashUnit(index, (int)(g_global_time * 11.0f) + 31);
    }
    spore->x_ratio = WrappedUnit(spore->x_ratio);
  }

  for (int index = 0; index < SHROOM_MENU_BACKGROUND_MUSHROOM_COUNT; ++index) {
    g_mushrooms[index].sway_phase += g_mushrooms[index].sway_speed * delta_time;
  }

  for (int index = 0; index < SHROOM_MENU_BACKGROUND_PARTICLE_COUNT; ++index) {
    MenuBackgroundParticle* particle = &g_particles[index];
    if (particle->life > 0.0f) {
      particle->x_ratio += particle->vx * delta_time;
      particle->y_ratio += particle->vy * delta_time;
      particle->vy += 0.10f * delta_time;
      particle->life -= delta_time;
    }
  }

  g_spawn_accumulator += delta_time;
  while (g_spawn_accumulator >= 0.18f) {
    const int seed = (int)(g_global_time * 1000.0f);
    SpawnParticle(HashUnit(seed, 41), HashUnit(seed, 43));
    g_spawn_accumulator -= 0.18f;
  }
}

static void DrawFungalMushroom(float x, float y, float scale, float sway, int type) {
  Color cap_color;
  Color stem_color;
  const float sway_offset = sinf(sway) * 12.0f * scale;

  if (type == 0) {
    cap_color = (Color){180, 100, 200, 150};
    stem_color = (Color){220, 200, 180, 150};
  } else if (type == 1) {
    cap_color = (Color){100, 180, 150, 150};
    stem_color = (Color){200, 220, 180, 150};
  } else {
    cap_color = (Color){200, 150, 100, 150};
    stem_color = (Color){220, 200, 180, 150};
  }

  DrawRectangle((int)(x - 8.0f * scale + sway_offset), (int)y, (int)(16.0f * scale),
                (int)(40.0f * scale), stem_color);
  DrawEllipse((int)(x + sway_offset), (int)y, 40.0f * scale, 30.0f * scale, cap_color);
  DrawEllipse((int)(x - 10.0f * scale + sway_offset), (int)(y - 5.0f * scale), 8.0f * scale,
              6.0f * scale, (Color){255, 255, 255, 100});
  DrawEllipse((int)(x + 12.0f * scale + sway_offset), (int)(y - 3.0f * scale), 6.0f * scale,
              5.0f * scale, (Color){255, 255, 255, 100});
}

static FungalMushroomPose GetMushroomPose(int mushroom_index, bool animate, int screen_width,
                                          int screen_height) {
  const int clamped_index = mushroom_index < 0
                                ? 0
                                : (mushroom_index >= SHROOM_MENU_BACKGROUND_MUSHROOM_COUNT
                                       ? SHROOM_MENU_BACKGROUND_MUSHROOM_COUNT - 1
                                       : mushroom_index);
  const MenuBackgroundMushroom* mushroom = &g_mushrooms[clamped_index];
  const float phase = (float)clamped_index * 0.73f;
  const float drift = animate
                          ? sinf(g_global_time * (0.45f + (float)clamped_index * 0.05f) + phase) *
                                SHROOM_MENU_BACKGROUND_MUSHROOM_DRIFT_RATIO
                          : 0.0f;
  const float bob =
      animate ? sinf(g_global_time * (0.75f + (float)clamped_index * 0.06f) + phase * 0.6f) *
                    SHROOM_MENU_BACKGROUND_MUSHROOM_BOB_RATIO
              : 0.0f;
  const float sway = animate ? mushroom->sway_phase : 0.0f;

  return (FungalMushroomPose){
      .global_time = g_global_time,
      .mushroom_x = WrappedUnit(mushroom->x_ratio + drift) * (float)screen_width,
      .mushroom_y = (mushroom->y_ratio + bob) * (float)screen_height,
      .mushroom_sway = sway,
  };
}

#ifdef TEST_MODE
ShroomFungalBackgroundDebugState ShroomScreenGetFungalBackgroundDebugState(int mushroom_index,
                                                                           bool animate,
                                                                           int screen_width,
                                                                           int screen_height) {
  if (!g_background_initialized) {
    ShroomScreenResetFungalBackground();
  }
  const FungalMushroomPose pose =
      GetMushroomPose(mushroom_index, animate, screen_width, screen_height);
  return (ShroomFungalBackgroundDebugState){
      .global_time = pose.global_time,
      .mushroom_x = pose.mushroom_x,
      .mushroom_y = pose.mushroom_y,
      .mushroom_sway = pose.mushroom_sway,
  };
}
#endif

void ShroomScreenDrawFungalBackground(bool animate) {
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();
  const float pulse = animate ? (0.5f + 0.5f * sinf(g_global_time * 0.5f)) : 0.0f;
  const Color gradient_top =
      (Color){(uint8_t)(30 + pulse * 16.0f), 20, (uint8_t)(50 + pulse * 24.0f), 255};
  const Color gradient_bottom =
      (Color){50, (uint8_t)(30 + pulse * 18.0f), (uint8_t)(70 + pulse * 20.0f), 255};

  if (!g_background_initialized) {
    ShroomScreenResetFungalBackground();
  }

  for (int y = 0; y < screen_height; y++) {
    const float t = screen_height > 0 ? (float)y / (float)screen_height : 0.0f;
    Color color;
    color.r = (uint8_t)(gradient_top.r + (gradient_bottom.r - gradient_top.r) * t);
    color.g = (uint8_t)(gradient_top.g + (gradient_bottom.g - gradient_top.g) * t);
    color.b = (uint8_t)(gradient_top.b + (gradient_bottom.b - gradient_top.b) * t);
    color.a = 255;
    DrawLine(0, y, screen_width, y, color);
  }

  for (int index = 0; index < SHROOM_MENU_BACKGROUND_MUSHROOM_COUNT; ++index) {
    const MenuBackgroundMushroom* mushroom = &g_mushrooms[index];
    const FungalMushroomPose pose = GetMushroomPose(index, animate, screen_width, screen_height);
    DrawFungalMushroom(pose.mushroom_x, pose.mushroom_y, mushroom->scale, pose.mushroom_sway,
                       mushroom->type);
  }

  for (int index = 0; index < SHROOM_MENU_BACKGROUND_PARTICLE_COUNT; ++index) {
    const MenuBackgroundParticle* particle = &g_particles[index];
    if (animate && (particle->life > 0.0f) && (particle->max_life > 0.0f)) {
      Color color = particle->color;
      color.a = (uint8_t)((particle->life / particle->max_life) * (float)color.a);
      DrawCircle((int)(particle->x_ratio * (float)screen_width),
                 (int)(particle->y_ratio * (float)screen_height), particle->size, color);
    }
  }

  for (int index = 0; index < SHROOM_MENU_BACKGROUND_SPORE_COUNT; ++index) {
    const MenuBackgroundSpore* spore = &g_spores[index];
    const Vector2 position = {spore->x_ratio * (float)screen_width,
                              spore->y_ratio * (float)screen_height};
    const Color color = {200, 180, 255, (uint8_t)(spore->alpha * 255.0f)};
    const Color glow_color = {220, 200, 255, (uint8_t)(spore->alpha * 100.0f)};
    DrawCircleV(position, spore->size * 1.7f, glow_color);
    DrawCircleV(position, spore->size, color);
  }
}
