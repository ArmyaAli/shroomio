#ifndef SHROOM_TEST_IMGUI_APP_H
#define SHROOM_TEST_IMGUI_APP_H

#include <limits.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "client/game.h"
#include "client/account_flow.h"
#include "client/screen.h"

#ifdef __cplusplus
}
#endif

typedef struct ShroomImGuiTestApp {
  ShroomScreenManager screen_manager;
  Game game;
  ShroomClientRest account_rest;
  ShroomAccountFlow account_flow;
  char temp_dir[PATH_MAX];
  bool screen_manager_initialized;
  bool account_flow_initialized;
  float frame_delta_override;
} ShroomImGuiTestApp;

extern ShroomImGuiTestApp g_imgui_test_app;

void ShroomImGuiTestAppReset(bool reset_files);
int ShroomImGuiTestAccountLoginCalls(void);
int ShroomImGuiTestAccountRegisterCalls(void);
int ShroomImGuiTestAccountLogoutCalls(void);

#endif
