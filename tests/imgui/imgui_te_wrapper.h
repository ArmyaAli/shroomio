#ifndef SHROOM_IMGUI_TE_WRAPPER_H
#define SHROOM_IMGUI_TE_WRAPPER_H

/* C API wrapper for the Dear ImGui Test Engine.
 *
 * All test driver code (main.c, tests.c) must use only this header and
 * imgui_wrapper.h.  No C++ headers or lambdas belong in test source files.
 *
 * The underlying ImGui Test Engine is C++; the implementation lives in
 * imgui_te_wrapper.cpp which is the only file that includes imgui_te_engine.h
 * and imgui_te_context.h directly.
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handles — test C code holds pointers but never dereferences them. */
typedef struct ImGuiTestEngine  ImGuiTestEngine;
typedef struct ImGuiTestContext ImGuiTestContext;

/* Function pointer type for test body functions. */
typedef void (*ShroomTeTestFunc)(ImGuiTestContext *ctx);

/* -------------------------------------------------------------------------
 * Engine lifecycle
 * ---------------------------------------------------------------------- */

/* Create the engine, configure headless/fast run IO, and bind to the current
 * ImGui context.  Must be called after ShroomImGui_Init(). */
ImGuiTestEngine *ShroomTeEngine_Create(void);

/* Queue all registered tests in ImGuiTestGroup_Tests. */
void ShroomTeEngine_QueueAll(ImGuiTestEngine *engine);

/* Stop the engine coroutine and prepare result collection. */
void ShroomTeEngine_Stop(ImGuiTestEngine *engine);

/* Destroy the engine.  Call after ImGui::DestroyContext(). */
void ShroomTeEngine_Destroy(ImGuiTestEngine *engine);

/* Per-frame hooks — call from the render loop. */
void ShroomTeEngine_PreSwap(ImGuiTestEngine *engine);
void ShroomTeEngine_PostSwap(ImGuiTestEngine *engine);

/* Returns true when the test queue is empty and no test is running. */
bool ShroomTeEngine_IsDone(ImGuiTestEngine *engine);

/* Fill *tested / *success / *queued with result counts. */
void ShroomTeEngine_GetResults(ImGuiTestEngine *engine, int *tested, int *success, int *queued);

/* -------------------------------------------------------------------------
 * Test registration
 * ---------------------------------------------------------------------- */

/* Register a test with a plain C function pointer (no lambdas needed). */
void ShroomTeEngine_RegisterTest(ImGuiTestEngine *engine, const char *category,
                                 const char *name, ShroomTeTestFunc fn);

/* -------------------------------------------------------------------------
 * ImGuiTestContext actions (called from inside test functions)
 * ---------------------------------------------------------------------- */

void ShroomTeCtx_SetRef(ImGuiTestContext *ctx, const char *ref);
bool ShroomTeCtx_SetRefWindow(ImGuiTestContext *ctx, const char *ref);
void ShroomTeCtx_ItemClick(ImGuiTestContext *ctx, const char *ref);
void ShroomTeCtx_ItemCheckbox(ImGuiTestContext *ctx, const char *ref);
void ShroomTeCtx_ItemInputValueStr(ImGuiTestContext *ctx, const char *ref, const char *value);
void ShroomTeCtx_ItemInputValueInt(ImGuiTestContext *ctx, const char *ref, int value);
void ShroomTeCtx_Yield(ImGuiTestContext *ctx, int count);
bool ShroomTeCtx_ItemExists(ImGuiTestContext *ctx, const char *ref);

/* -------------------------------------------------------------------------
 * ImGui window queries (hides ImGuiWindow* from C code)
 * ---------------------------------------------------------------------- */

/* Returns true if an ImGui window with this name exists and is active. */
bool ShroomTeImGui_WindowIsActive(const char *name);

/* Returns true if the given window is the current nav (keyboard-focus) window. */
bool ShroomTeImGui_WindowIsNavFocused(const char *name);

/* -------------------------------------------------------------------------
 * Assertion helpers — called by the macros below.
 * Reports pass/fail through the test engine's own logging.
 * ---------------------------------------------------------------------- */

bool ShroomTe_Check(const char *file, const char *func, int line,
                    bool result, const char *expr);
bool ShroomTe_CheckEq(const char *file, const char *func, int line,
                      long long a, long long b,
                      const char *a_expr, const char *b_expr);
bool ShroomTe_CheckStrEq(const char *file, const char *func, int line,
                         const char *a, const char *b,
                         const char *a_expr, const char *b_expr);

/* -------------------------------------------------------------------------
 * C-compatible check macros (mirror the C++ IM_CHECK_* names)
 * ---------------------------------------------------------------------- */

/* Guard against redefinition: when imgui_te_context.h is already included
 * (as it is in imgui_te_wrapper.cpp) the native C++ macros take precedence.
 * For plain C compilation units these will always be defined here. */
#ifndef IM_CHECK
#define IM_CHECK(expr) \
    ShroomTe_Check(__FILE__, __func__, __LINE__, (bool)(expr), #expr)
#endif

#ifndef IM_CHECK_EQ
#define IM_CHECK_EQ(a, b) \
    ShroomTe_CheckEq(__FILE__, __func__, __LINE__, \
                     (long long)(a), (long long)(b), #a, #b)
#endif

#ifndef IM_CHECK_STR_EQ
#define IM_CHECK_STR_EQ(a, b) \
    ShroomTe_CheckStrEq(__FILE__, __func__, __LINE__, (a), (b), #a, #b)
#endif

#ifdef __cplusplus
}
#endif

#endif /* SHROOM_IMGUI_TE_WRAPPER_H */
