#include "game.h"
#include "screen.h"

#include "imgui_wrapper.h"
#include "raylib.h"

#include <math.h>
#include <stdlib.h>

#define MAX_SPORES 50
#define MAX_MUSHROOMS 8
#define MAX_PARTICLES 100

typedef struct {
  float x;
  float y;
  float speed;
  float size;
  float alpha;
  float phase;
} Spore;

typedef struct {
  float x;
  float y;
  float scale;
  float sway_phase;
  float sway_speed;
  int type;
} Mushroom;

typedef struct {
  float x;
  float y;
  float vx;
  float vy;
  float life;
  float max_life;
  float size;
  Color color;
} Particle;

static Spore spores[MAX_SPORES];
static Mushroom mushrooms[MAX_MUSHROOMS];
static Particle particles[MAX_PARTICLES];
static float global_time = 0.0f;

static void InitBackground(void) {
  for (int i = 0; i < MAX_SPORES; i++) {
    spores[i].x = (float)(rand() % 1280);
    spores[i].y = (float)(rand() % 720);
    spores[i].speed = 20.0f + (float)(rand() % 30);
    spores[i].size = 2.0f + (float)(rand() % 4);
    spores[i].alpha = 0.3f + (float)(rand() % 5) / 10.0f;
    spores[i].phase = (float)(rand() % 100) / 10.0f;
  }

  for (int i = 0; i < MAX_MUSHROOMS; i++) {
    mushrooms[i].x = (float)(rand() % 1280);
    mushrooms[i].y = 500.0f + (float)(rand() % 220);
    mushrooms[i].scale = 0.5f + (float)(rand() % 10) / 10.0f;
    mushrooms[i].sway_phase = (float)(rand() % 100) / 10.0f;
    mushrooms[i].sway_speed = 0.5f + (float)(rand() % 10) / 20.0f;
    mushrooms[i].type = rand() % 3;
  }

  for (int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].life = 0.0f;
  }
}

static void SpawnParticle(float x, float y) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].life <= 0.0f) {
      particles[i].x = x;
      particles[i].y = y;
      particles[i].vx = ((float)(rand() % 100) - 50.0f) / 50.0f;
      particles[i].vy = -((float)(rand() % 50) + 20.0f) / 50.0f;
      particles[i].life = 2.0f + (float)(rand() % 20) / 10.0f;
      particles[i].max_life = particles[i].life;
      particles[i].size = 1.0f + (float)(rand() % 3);
      particles[i].color = (Color){200, 150, 255, 200};
      break;
    }
  }
}

static void UpdateBackground(float delta_time) {
  global_time += delta_time;

  for (int i = 0; i < MAX_SPORES; i++) {
    spores[i].y -= spores[i].speed * delta_time;
    spores[i].x += sinf(global_time + spores[i].phase) * 10.0f * delta_time;

    if (spores[i].y < -10.0f) {
      spores[i].y = 730.0f;
      spores[i].x = (float)(rand() % 1280);
    }
  }

  for (int i = 0; i < MAX_MUSHROOMS; i++) {
    mushrooms[i].sway_phase += mushrooms[i].sway_speed * delta_time;
  }

  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].life > 0.0f) {
      particles[i].x += particles[i].vx * 60.0f * delta_time;
      particles[i].y += particles[i].vy * 60.0f * delta_time;
      particles[i].vy += 0.5f * delta_time;
      particles[i].life -= delta_time;
    }
  }

  if (rand() % 100 < 5) {
    SpawnParticle((float)(rand() % 1280), (float)(rand() % 720));
  }
}

static void DrawMushroom(float x, float y, float scale, float sway, int type) {
  Color cap_color, stem_color;

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

  float sway_offset = sinf(sway) * 5.0f * scale;

  DrawRectangle(x - 8.0f * scale + sway_offset, y, 16.0f * scale, 40.0f * scale, stem_color);

  DrawEllipse(x + sway_offset, y, 40.0f * scale, 30.0f * scale, cap_color);

  DrawEllipse(x - 10.0f * scale + sway_offset, y - 5.0f * scale, 8.0f * scale, 6.0f * scale,
              (Color){255, 255, 255, 100});
  DrawEllipse(x + 12.0f * scale + sway_offset, y - 3.0f * scale, 6.0f * scale, 5.0f * scale,
              (Color){255, 255, 255, 100});
}

static void DrawBackground(void) {
  Color gradient_top = (Color){30, 20, 50, 255};
  Color gradient_bottom = (Color){50, 30, 70, 255};

  for (int y = 0; y < 720; y++) {
    float t = (float)y / 720.0f;
    Color c;
    c.r = (uint8_t)(gradient_top.r + (gradient_bottom.r - gradient_top.r) * t);
    c.g = (uint8_t)(gradient_top.g + (gradient_bottom.g - gradient_top.g) * t);
    c.b = (uint8_t)(gradient_top.b + (gradient_bottom.b - gradient_top.b) * t);
    c.a = 255;
    DrawLine(0, y, 1280, y, c);
  }

  for (int i = 0; i < MAX_MUSHROOMS; i++) {
    DrawMushroom(mushrooms[i].x, mushrooms[i].y, mushrooms[i].scale, mushrooms[i].sway_phase,
                 mushrooms[i].type);
  }

  for (int i = 0; i < MAX_SPORES; i++) {
    Color spore_color = {200, 180, 255, (uint8_t)(spores[i].alpha * 255)};
    DrawCircle((int)spores[i].x, (int)spores[i].y, spores[i].size, spore_color);

    Color glow_color = {220, 200, 255, (uint8_t)(spores[i].alpha * 100)};
    DrawCircle((int)spores[i].x, (int)spores[i].y, spores[i].size * 1.5f, glow_color);
  }

  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].life > 0.0f) {
      float alpha = particles[i].life / particles[i].max_life;
      Color p_color = particles[i].color;
      p_color.a = (uint8_t)(alpha * p_color.a);
      DrawCircle((int)particles[i].x, (int)particles[i].y, particles[i].size, p_color);
    }
  }
}

static bool MainMenuInit(ShroomScreenManager* manager) {
  (void)manager;
  InitBackground();
  return true;
}

static void MainMenuUpdate(ShroomScreenManager* manager, float delta_time) {
  (void)manager;
  UpdateBackground(delta_time);
}

static void MainMenuDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();
  const float panel_width = 340.0f;
  const float panel_height = 470.0f;

  DrawBackground();

  ShroomImGui_SetNextWindowPos((screen_width - (int)panel_width) * 0.5f,
                               (screen_height - (int)panel_height) * 0.5f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(panel_width, panel_height, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.85f);
  if (!ShroomImGui_Begin("Main Menu", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_Text("SHROOMIO");
  ShroomImGui_TextWrapped(
      "Grow by collecting spores, out-position bigger threats, and take over the arena.");
  ShroomImGui_Spacing();

  if (ShroomImGui_Button("Play Online", -1.0f, 38.0f)) {
    if (game != NULL) {
      ClientNetInit(&game->net, game->selected_server_host, game->selected_server_port);
      game->auto_join_lobby = true;
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_LOBBY);
  }
  if (ShroomImGui_Button("Custom Server", -1.0f, 38.0f)) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }
  if (ShroomImGui_Button("Offline Practice", -1.0f, 38.0f)) {
    if (game != NULL) {
      game->selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
  }
  if (ShroomImGui_Button("Settings", -1.0f, 38.0f)) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SETTINGS);
  }
  if (ShroomImGui_Button("Help", -1.0f, 38.0f)) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_HELP);
  }
  if (ShroomImGui_Button("Credits", -1.0f, 38.0f)) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_CREDITS);
  }
  if (ShroomImGui_Button("Exit", -1.0f, 38.0f)) {
    ShroomScreenManagerRequestExit(manager);
  }

  ShroomImGui_End();
}

static void MainMenuHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerRequestExit(manager);
  }
}

static void MainMenuCleanup(ShroomScreenManager* manager) { (void)manager; }

void ShroomScreenRegisterMainMenu(ShroomScreenManager* manager) {
  ShroomScreen* screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_MAIN_MENU];
  screen->type = SHROOM_SCREEN_MAIN_MENU;
  screen->name = "Main Menu";
  screen->init = MainMenuInit;
  screen->update = MainMenuUpdate;
  screen->draw = MainMenuDraw;
  screen->handle_input = MainMenuHandleInput;
  screen->cleanup = MainMenuCleanup;
}
