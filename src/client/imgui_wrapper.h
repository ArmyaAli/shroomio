#ifndef SHROOM_IMGUI_WRAPPER_H
#define SHROOM_IMGUI_WRAPPER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ShroomImGuiColor {
  float r;
  float g;
  float b;
  float a;
} ShroomImGuiColor;

enum {
  SHROOM_IMGUI_COND_NONE = 0,
  SHROOM_IMGUI_COND_ALWAYS = 1 << 0,
  SHROOM_IMGUI_COND_ONCE = 1 << 1,
};

enum {
  SHROOM_IMGUI_WINDOW_NO_TITLE_BAR = 1 << 0,
  SHROOM_IMGUI_WINDOW_NO_RESIZE = 1 << 1,
  SHROOM_IMGUI_WINDOW_NO_MOVE = 1 << 2,
  SHROOM_IMGUI_WINDOW_NO_COLLAPSE = 1 << 3,
  SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS = 1 << 4,
  SHROOM_IMGUI_WINDOW_NO_SCROLLBAR = 1 << 5,
  SHROOM_IMGUI_WINDOW_NO_SCROLL_WITH_MOUSE = 1 << 6,
  SHROOM_IMGUI_WINDOW_ALWAYS_AUTO_RESIZE = 1 << 7,
};

enum {
  SHROOM_IMGUI_TABLE_BORDERS = 1 << 0,
  SHROOM_IMGUI_TABLE_ROW_BG = 1 << 1,
  SHROOM_IMGUI_TABLE_SCROLL_Y = 1 << 2,
  SHROOM_IMGUI_TABLE_SIZING_STRETCH = 1 << 3,
  SHROOM_IMGUI_TABLE_SIZING_FIXED = 1 << 4, /* allows explicit pixel widths per column */
};

enum {
  SHROOM_IMGUI_SELECTABLE_SPAN_ALL_COLUMNS = 1 << 0,
};

void ShroomImGui_Init(void);
void ShroomImGui_Shutdown(void);
void ShroomImGui_NewFrame(void);
void ShroomImGui_Render(void);
void ShroomImGui_SetUiScale(float scale);
void ShroomImGui_ApplyTheme(bool high_contrast);

void ShroomImGui_SetNextWindowPos(float x, float y, int condition);
void ShroomImGui_SetNextWindowSize(float width, float height, int condition);
void ShroomImGui_SetNextWindowBgAlpha(float alpha);
bool ShroomImGui_Begin(const char* name, bool* open, int flags);
void ShroomImGui_End(void);

void ShroomImGui_SetNextItemWidth(float width);
void ShroomImGui_Text(const char* text);
void ShroomImGui_TextWrapped(const char* text);
void ShroomImGui_TextColored(ShroomImGuiColor color, const char* text);
void ShroomImGui_Separator(void);
void ShroomImGui_Spacing(void);
void ShroomImGui_SameLine(void);

bool ShroomImGui_Button(const char* label, float width, float height);
bool ShroomImGui_Checkbox(const char* label, bool* value);
bool ShroomImGui_SliderInt(const char* label, int* value, int minimum, int maximum,
                           const char* format);
bool ShroomImGui_Combo(const char* label, int* current_item, const char* const items[],
                       int items_count);
bool ShroomImGui_InputText(const char* label, char* buffer, size_t buffer_size);

bool ShroomImGui_BeginChild(const char* id, float width, float height, bool border);
void ShroomImGui_EndChild(void);
void ShroomImGui_SetScrollHereY(float center_y_ratio);
bool ShroomImGui_WantCaptureKeyboard(void);
void ShroomImGui_SetKeyboardFocusHere(void);
void ShroomImGui_PushWindowRounding(float rounding);
void ShroomImGui_PushWindowPadding(float x, float y);
void ShroomImGui_PopStyleVar(void);

/* ImGuiCol_* ordinals for the subset we expose. */
enum {
  SHROOM_IMGUI_COL_WINDOW_BG = 0,
  SHROOM_IMGUI_COL_BUTTON = 19,
  SHROOM_IMGUI_COL_BUTTON_HOVERED = 20,
  SHROOM_IMGUI_COL_BUTTON_ACTIVE = 21,
};
void ShroomImGui_PushStyleColor(int col, float r, float g, float b, float a);
void ShroomImGui_PopStyleColor(void);
/* Returns true when the user presses Enter or clicks the submit button. */
bool ShroomImGui_InputTextWithSubmit(const char* label, char* buffer, size_t buffer_size,
                                     const char* submit_label);

bool ShroomImGui_BeginTable(const char* id, int columns, int flags, float width, float height);
void ShroomImGui_EndTable(void);
void ShroomImGui_TableSetupColumn(const char* label, float width);
void ShroomImGui_TableHeadersRow(void);
void ShroomImGui_TableNextRow(void);
void ShroomImGui_TableSetColumnIndex(int index);
bool ShroomImGui_Selectable(const char* label, bool selected, int flags, float width, float height);

#ifdef __cplusplus
}
#endif

#endif
