/* imgui_te_wrapper.cpp
 *
 * C++ implementation of the C API declared in imgui_te_wrapper.h.
 * This is the only file in tests/imgui/ that may include ImGui Test Engine
 * C++ headers directly.  All test driver code (main.c, tests.c) goes through
 * the C API and never sees these headers.
 *
 * C++ engine headers are included first so that the #ifndef guards in
 * imgui_te_wrapper.h defer to the native C++ macro definitions instead of
 * triggering redefinition warnings.
 */

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

#include "imgui_te_wrapper.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Engine lifecycle
 * ---------------------------------------------------------------------- */

ImGuiTestEngine *ShroomTeEngine_Create(void) {
  ImGuiTestEngine *engine = ImGuiTestEngine_CreateContext();
  ImGuiTestEngineIO &io = ImGuiTestEngine_GetIO(engine);
  io.ConfigSavedSettings = false;
  io.ConfigCaptureEnabled = false;
  io.ConfigNoThrottle = true;
  io.ConfigLogToTTY = true;
  io.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;
  io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
  io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
  if (getenv("SHROOM_VALGRIND_IMGUI") != NULL) {
    io.ConfigWatchdogWarning = FLT_MAX;
    io.ConfigWatchdogKillTest = FLT_MAX;
  }

  ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
  ImGuiTestEngine_InstallDefaultCrashHandler();
  return engine;
}

void ShroomTeEngine_QueueAll(ImGuiTestEngine *engine) {
  const char *filter = getenv("SHROOM_IMGUI_TEST_FILTER");
  ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests,
                             (filter != NULL && filter[0] != '\0') ? filter : NULL);
}

void ShroomTeEngine_Stop(ImGuiTestEngine *engine) {
  ImGuiTestEngine_Stop(engine);
}

void ShroomTeEngine_Destroy(ImGuiTestEngine *engine) {
  ImGuiTestEngine_DestroyContext(engine);
}

void ShroomTeEngine_PreSwap(ImGuiTestEngine *engine) {
  ImGuiTestEngine_PreSwap(engine);
}

void ShroomTeEngine_PostSwap(ImGuiTestEngine *engine) {
  ImGuiTestEngine_PostSwap(engine);
}

bool ShroomTeEngine_IsDone(ImGuiTestEngine *engine) {
  const ImGuiTestEngineIO &io = ImGuiTestEngine_GetIO(engine);
  return ImGuiTestEngine_IsTestQueueEmpty(engine) && !io.IsRunningTests;
}

void ShroomTeEngine_GetResults(ImGuiTestEngine *engine, int *tested, int *success, int *queued) {
  ImGuiTestEngineResultSummary summary;
  ImGuiTestEngine_GetResultSummary(engine, &summary);
  if (tested) *tested = summary.CountTested;
  if (success) *success = summary.CountSuccess;
  if (queued) *queued = summary.CountInQueue;
}

/* -------------------------------------------------------------------------
 * Test registration
 * ---------------------------------------------------------------------- */

void ShroomTeEngine_RegisterTest(ImGuiTestEngine *engine, const char *category,
                                 const char *name, ShroomTeTestFunc fn) {
  ImGuiTest *test = ImGuiTestEngine_RegisterTest(engine, category, name);
  test->TestFunc = fn;
}

/* -------------------------------------------------------------------------
 * ImGuiTestContext actions
 * ---------------------------------------------------------------------- */

void ShroomTeCtx_SetRef(ImGuiTestContext *ctx, const char *ref) {
  ctx->SetRef(ref);
}

void ShroomTeCtx_ItemClick(ImGuiTestContext *ctx, const char *ref) {
  ctx->ItemClick(ref);
}

void ShroomTeCtx_ItemCheckbox(ImGuiTestContext *ctx, const char *ref) {
  ctx->ItemCheck(ref);
}

void ShroomTeCtx_ItemInputValueStr(ImGuiTestContext *ctx, const char *ref,
                                   const char *value) {
  ctx->ItemInputValue(ref, value);
}

void ShroomTeCtx_ItemInputValueInt(ImGuiTestContext *ctx, const char *ref,
                                   int value) {
  ctx->ItemInputValue(ref, value);
}

void ShroomTeCtx_Yield(ImGuiTestContext *ctx, int count) {
  ctx->Yield(count);
}

bool ShroomTeCtx_ItemExists(ImGuiTestContext *ctx, const char *ref) {
  return ctx->ItemExists(ref);
}

/* -------------------------------------------------------------------------
 * ImGui window queries
 * ---------------------------------------------------------------------- */

bool ShroomTeImGui_WindowIsActive(const char *name) {
  ImGuiWindow *w = ImGui::FindWindowByName(name);
  return w != NULL && w->Active;
}

bool ShroomTeImGui_WindowIsNavFocused(const char *name) {
  ImGuiWindow *w = ImGui::FindWindowByName(name);
  if (w == NULL) return false;
  return ImGui::GetCurrentContext()->NavWindow == w;
}

/* -------------------------------------------------------------------------
 * Assertion helpers
 * ---------------------------------------------------------------------- */

bool ShroomTe_Check(const char *file, const char *func, int line,
                    bool result, const char *expr) {
  return ImGuiTestEngine_Check(file, func, line, ImGuiTestCheckFlags_None,
                               result, expr);
}

bool ShroomTe_CheckEq(const char *file, const char *func, int line,
                      long long a, long long b,
                      const char *a_expr, const char *b_expr) {
  char desc[512];
  snprintf(desc, sizeof(desc), "%s == %s (%lld == %lld)", a_expr, b_expr, a, b);
  return ImGuiTestEngine_Check(file, func, line, ImGuiTestCheckFlags_None,
                               a == b, desc);
}

bool ShroomTe_CheckStrEq(const char *file, const char *func, int line,
                         const char *a, const char *b,
                         const char *a_expr, const char *b_expr) {
  bool ok = (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
  char desc[512];
  snprintf(desc, sizeof(desc), "%s == %s (\"%s\" == \"%s\")",
           a_expr, b_expr,
           a ? a : "(null)", b ? b : "(null)");
  return ImGuiTestEngine_Check(file, func, line, ImGuiTestCheckFlags_None,
                               ok, desc);
}
