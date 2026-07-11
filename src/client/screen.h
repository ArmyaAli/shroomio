#ifndef SHROOM_SCREEN_H
#define SHROOM_SCREEN_H

#include <stdbool.h>

typedef enum ShroomScreenType {
  SHROOM_SCREEN_NONE = 0,
  SHROOM_SCREEN_MAIN_MENU,
  SHROOM_SCREEN_AUTH,
  SHROOM_SCREEN_GAME_MODE_SELECT,
  SHROOM_SCREEN_SERVER_BROWSER,
  SHROOM_SCREEN_LOBBY,
  SHROOM_SCREEN_LOBBY_ROSTER,
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
void ShroomScreenRegisterSettings(ShroomScreenManager* manager);
void ShroomScreenRegisterHelp(ShroomScreenManager* manager);
void ShroomScreenRegisterCredits(ShroomScreenManager* manager);
void ShroomScreenRegisterGameModeSelect(ShroomScreenManager* manager);
void ShroomScreenRegisterServerBrowser(ShroomScreenManager* manager);
void ShroomScreenRegisterLobbyBrowser(ShroomScreenManager* manager);
void ShroomScreenRegisterLobbyRoster(ShroomScreenManager* manager);
void ShroomScreenRegisterGame(ShroomScreenManager* manager);
void ShroomScreenRegisterResults(ShroomScreenManager* manager);

#ifdef TEST_MODE
const char* ShroomTestGetServerBrowserValidationMessage(void);
int ShroomTestGetServerBrowserSelectedIndex(void);
const char* ShroomTestGetServerBrowserSelectedHost(void);
int ShroomTestGetServerBrowserRecentCount(void);
int ShroomTestGetServerBrowserDiscoveryState(void);
int ShroomTestGetServerBrowserServerCount(void);
bool ShroomTestGetServerBrowserSortDescending(void);
void ShroomTestMarkServerBrowserStale(void);
void ShroomTestCompleteServerBrowserRefresh(bool demo_mode);

struct Game;
const char* ShroomTestGetResultsDurationText(const struct Game* game);
bool ShroomTestMainMenuAnimationsEnabled(const struct Game* game);
void ShroomTestSettingsEscape(ShroomScreenManager* manager);
void ShroomTestSettingsBeginRebind(int slot);
void ShroomTestSettingsCaptureKey(int key);
int ShroomTestSettingsPendingKey(int slot);
const char* ShroomTestGetSettingsRebindError(void);
#endif

#endif
