#include "screen.h"
#include <stddef.h>
#include <string.h>

void ShroomScreenManagerInit(ShroomScreenManager* manager) {
  if (manager == NULL) {
    return;
  }
  memset(manager, 0, sizeof(ShroomScreenManager));
  manager->running = true;
  manager->current_screen = NULL;
  manager->previous_screen = NULL;
}

void ShroomScreenManagerShutdown(ShroomScreenManager* manager) {
  if (manager == NULL) {
    return;
  }
  if (manager->current_screen != NULL && manager->current_screen->cleanup != NULL) {
    manager->current_screen->cleanup(manager);
  }
  manager->current_screen = NULL;
  manager->previous_screen = NULL;
  manager->running = false;
}

bool ShroomScreenManagerTransition(ShroomScreenManager* manager, ShroomScreenType new_screen) {
  if (manager == NULL || new_screen <= SHROOM_SCREEN_NONE || new_screen >= SHROOM_SCREEN_COUNT) {
    return false;
  }

  ShroomScreen* target = &manager->screens[new_screen];
  if (target->type != new_screen) {
    return false;
  }

  if (manager->current_screen != NULL && manager->current_screen->cleanup != NULL) {
    manager->current_screen->cleanup(manager);
  }

  manager->previous_screen = manager->current_screen;
  manager->current_screen = target;

  if (target->init != NULL) {
    if (!target->init(manager)) {
      manager->current_screen = manager->previous_screen;
      manager->previous_screen = NULL;
      return false;
    }
  }

  return true;
}

bool ShroomScreenManagerGoBack(ShroomScreenManager* manager) {
  if (manager == NULL || manager->previous_screen == NULL) {
    return false;
  }

  if (manager->current_screen != NULL && manager->current_screen->cleanup != NULL) {
    manager->current_screen->cleanup(manager);
  }

  ShroomScreen* back_screen = manager->previous_screen;
  manager->previous_screen = NULL;
  manager->current_screen = back_screen;

  if (back_screen->init != NULL) {
    if (!back_screen->init(manager)) {
      manager->current_screen = NULL;
      return false;
    }
  }

  return true;
}

void ShroomScreenManagerUpdate(ShroomScreenManager* manager, float delta_time) {
  if (manager == NULL || manager->current_screen == NULL) {
    return;
  }
  if (manager->current_screen->update != NULL) {
    manager->current_screen->update(manager, delta_time);
  }
}

void ShroomScreenManagerDraw(ShroomScreenManager* manager) {
  if (manager == NULL || manager->current_screen == NULL) {
    return;
  }
  if (manager->current_screen->draw != NULL) {
    manager->current_screen->draw(manager);
  }
}

void ShroomScreenManagerHandleInput(ShroomScreenManager* manager) {
  if (manager == NULL || manager->current_screen == NULL) {
    return;
  }
  if (manager->current_screen->handle_input != NULL) {
    manager->current_screen->handle_input(manager);
  }
}

ShroomScreenType ShroomScreenManagerGetCurrentScreen(const ShroomScreenManager* manager) {
  if (manager == NULL || manager->current_screen == NULL) {
    return SHROOM_SCREEN_NONE;
  }
  return manager->current_screen->type;
}

const char* ShroomScreenManagerGetCurrentScreenName(const ShroomScreenManager* manager) {
  if (manager == NULL || manager->current_screen == NULL) {
    return "NONE";
  }
  return manager->current_screen->name;
}

bool ShroomScreenManagerIsRunning(const ShroomScreenManager* manager) {
  if (manager == NULL) {
    return false;
  }
  return manager->running;
}

void ShroomScreenManagerRequestExit(ShroomScreenManager* manager) {
  if (manager == NULL) {
    return;
  }
  manager->running = false;
}

const char* ShroomScreenTypeToString(ShroomScreenType type) {
  switch (type) {
  case SHROOM_SCREEN_NONE:
    return "NONE";
  case SHROOM_SCREEN_MAIN_MENU:
    return "MAIN_MENU";
  case SHROOM_SCREEN_AUTH:
    return "AUTH";
  case SHROOM_SCREEN_GAME_MODE_SELECT:
    return "GAME_MODE_SELECT";
  case SHROOM_SCREEN_SERVER_BROWSER:
    return "SERVER_BROWSER";
  case SHROOM_SCREEN_LOBBY:
    return "LOBBY";
  case SHROOM_SCREEN_GAME:
    return "GAME";
  case SHROOM_SCREEN_RESULTS:
    return "RESULTS";
  case SHROOM_SCREEN_SETTINGS:
    return "SETTINGS";
  case SHROOM_SCREEN_HELP:
    return "HELP";
  case SHROOM_SCREEN_CREDITS:
    return "CREDITS";
  case SHROOM_SCREEN_COUNT:
    return "COUNT";
  default:
    return "UNKNOWN";
  }
}
