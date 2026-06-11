#ifndef SHROOM_SCREEN_H
#define SHROOM_SCREEN_H

#include <stdbool.h>

typedef enum ShroomScreenType {
  SHROOM_SCREEN_NONE = 0,
  SHROOM_SCREEN_MAIN_MENU,
  SHROOM_SCREEN_AUTH,
  SHROOM_SCREEN_SERVER_BROWSER,
  SHROOM_SCREEN_LOBBY,
  SHROOM_SCREEN_GAME,
  SHROOM_SCREEN_RESULTS,
  SHROOM_SCREEN_SETTINGS,
  SHROOM_SCREEN_HELP,
  SHROOM_SCREEN_CREDITS,
  SHROOM_SCREEN_COUNT
} ShroomScreenType;

typedef struct ShroomScreenManager ShroomScreenManager;

typedef struct ShroomScreen {
  ShroomScreenType type;
  const char* name;
  bool (*init)(ShroomScreenManager* manager);
  void (*update)(ShroomScreenManager* manager, float delta_time);
  void (*draw)(ShroomScreenManager* manager);
  void (*handle_input)(ShroomScreenManager* manager);
  void (*cleanup)(ShroomScreenManager* manager);
} ShroomScreen;

struct ShroomScreenManager {
  ShroomScreen* current_screen;
  ShroomScreen* previous_screen;
  ShroomScreen screens[SHROOM_SCREEN_COUNT];
  bool running;
  void* user_data;
};

void ShroomScreenManagerInit(ShroomScreenManager* manager);
void ShroomScreenManagerShutdown(ShroomScreenManager* manager);

bool ShroomScreenManagerTransition(ShroomScreenManager* manager, ShroomScreenType new_screen);
bool ShroomScreenManagerGoBack(ShroomScreenManager* manager);

void ShroomScreenManagerUpdate(ShroomScreenManager* manager, float delta_time);
void ShroomScreenManagerDraw(ShroomScreenManager* manager);
void ShroomScreenManagerHandleInput(ShroomScreenManager* manager);

ShroomScreenType ShroomScreenManagerGetCurrentScreen(const ShroomScreenManager* manager);
const char* ShroomScreenManagerGetCurrentScreenName(const ShroomScreenManager* manager);

bool ShroomScreenManagerIsRunning(const ShroomScreenManager* manager);
void ShroomScreenManagerRequestExit(ShroomScreenManager* manager);

const char* ShroomScreenTypeToString(ShroomScreenType type);

void ShroomScreenRegisterMainMenu(ShroomScreenManager* manager);

#endif
