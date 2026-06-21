#include "imgui_wrapper.h"

#include "imgui.h"
#include "imgui_impl_raylib.h"

static ImGuiCond ToImGuiCond(int condition) {
  if ((condition & SHROOM_IMGUI_COND_ALWAYS) != 0) {
    return ImGuiCond_Always;
  }
  if ((condition & SHROOM_IMGUI_COND_ONCE) != 0) {
    return ImGuiCond_Once;
  }
  return ImGuiCond_None;
}

static ImGuiWindowFlags ToImGuiWindowFlags(int flags) {
  ImGuiWindowFlags imgui_flags = 0;

  if ((flags & SHROOM_IMGUI_WINDOW_NO_TITLE_BAR) != 0) {
    imgui_flags |= ImGuiWindowFlags_NoTitleBar;
  }
  if ((flags & SHROOM_IMGUI_WINDOW_NO_RESIZE) != 0) {
    imgui_flags |= ImGuiWindowFlags_NoResize;
  }
  if ((flags & SHROOM_IMGUI_WINDOW_NO_MOVE) != 0) {
    imgui_flags |= ImGuiWindowFlags_NoMove;
  }
  if ((flags & SHROOM_IMGUI_WINDOW_NO_COLLAPSE) != 0) {
    imgui_flags |= ImGuiWindowFlags_NoCollapse;
  }
  if ((flags & SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS) != 0) {
    imgui_flags |= ImGuiWindowFlags_NoSavedSettings;
  }
  if ((flags & SHROOM_IMGUI_WINDOW_NO_SCROLLBAR) != 0) {
    imgui_flags |= ImGuiWindowFlags_NoScrollbar;
  }
  if ((flags & SHROOM_IMGUI_WINDOW_NO_SCROLL_WITH_MOUSE) != 0) {
    imgui_flags |= ImGuiWindowFlags_NoScrollWithMouse;
  }
  if ((flags & SHROOM_IMGUI_WINDOW_ALWAYS_AUTO_RESIZE) != 0) {
    imgui_flags |= ImGuiWindowFlags_AlwaysAutoResize;
  }

  return imgui_flags;
}

static ImGuiTableFlags ToImGuiTableFlags(int flags) {
  ImGuiTableFlags imgui_flags = 0;

  if ((flags & SHROOM_IMGUI_TABLE_BORDERS) != 0) {
    imgui_flags |= ImGuiTableFlags_Borders;
  }
  if ((flags & SHROOM_IMGUI_TABLE_ROW_BG) != 0) {
    imgui_flags |= ImGuiTableFlags_RowBg;
  }
  if ((flags & SHROOM_IMGUI_TABLE_SCROLL_Y) != 0) {
    imgui_flags |= ImGuiTableFlags_ScrollY;
  }
  if ((flags & SHROOM_IMGUI_TABLE_SIZING_STRETCH) != 0) {
    imgui_flags |= ImGuiTableFlags_SizingStretchSame;
  }
  if ((flags & SHROOM_IMGUI_TABLE_SIZING_FIXED) != 0) {
    imgui_flags |= ImGuiTableFlags_SizingFixedFit;
  }

  return imgui_flags;
}

static ImGuiSelectableFlags ToImGuiSelectableFlags(int flags) {
  ImGuiSelectableFlags imgui_flags = 0;

  if ((flags & SHROOM_IMGUI_SELECTABLE_SPAN_ALL_COLUMNS) != 0) {
    imgui_flags |= ImGuiSelectableFlags_SpanAllColumns;
  }

  return imgui_flags;
}

extern "C" {

void ShroomImGui_Init(void) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplRaylib_Init();
}

void ShroomImGui_Shutdown(void) {
  ImGui_ImplRaylib_Shutdown();
  ImGui::DestroyContext();
}

void ShroomImGui_NewFrame(void) {
  ImGui_ImplRaylib_NewFrame();
  ImGui::NewFrame();
}

void ShroomImGui_Render(void) {
  ImGui::Render();
  ImGui_ImplRaylib_RenderDrawData(ImGui::GetDrawData());
}

void ShroomImGui_SetUiScale(float scale) { ImGui::GetIO().FontGlobalScale = scale; }

void ShroomImGui_ApplyTheme(bool high_contrast) {
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors;

  ImGui::StyleColorsDark();
  style.WindowPadding = ImVec2(16.0f, 14.0f);
  style.FramePadding = ImVec2(10.0f, 7.0f);
  style.ItemSpacing = ImVec2(10.0f, 8.0f);
  style.WindowRounding = 14.0f;
  style.ChildRounding = 12.0f;
  style.FrameRounding = 11.0f;
  style.PopupRounding = 12.0f;
  style.GrabRounding = 10.0f;
  style.ScrollbarRounding = 10.0f;
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;

  colors = style.Colors;
  colors[ImGuiCol_Text] = ImVec4(0.96f, 0.92f, 0.82f, 1.0f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.58f, 0.52f, 0.44f, 1.0f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.08f, 0.07f, 0.94f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.10f, 0.08f, 0.90f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.09f, 0.07f, 0.96f);
  colors[ImGuiCol_Border] = ImVec4(0.62f, 0.42f, 0.23f, 0.58f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.24f, 0.16f, 0.12f, 0.92f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.42f, 0.28f, 0.17f, 0.96f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.39f, 0.20f, 1.0f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.18f, 0.12f, 0.10f, 1.0f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.32f, 0.20f, 0.13f, 1.0f);
  colors[ImGuiCol_Button] = ImVec4(0.49f, 0.29f, 0.18f, 0.96f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.72f, 0.45f, 0.22f, 1.0f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.86f, 0.64f, 0.29f, 1.0f);
  colors[ImGuiCol_Header] = ImVec4(0.30f, 0.45f, 0.25f, 0.86f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.43f, 0.63f, 0.34f, 0.96f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.55f, 0.78f, 0.42f, 1.0f);
  colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.89f, 0.44f, 1.0f);
  colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.78f, 0.32f, 1.0f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.74f, 0.42f, 0.78f, 1.0f);
  colors[ImGuiCol_Separator] = ImVec4(0.62f, 0.42f, 0.23f, 0.52f);
  colors[ImGuiCol_Tab] = ImVec4(0.22f, 0.16f, 0.12f, 1.0f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.46f, 0.31f, 0.19f, 1.0f);
  colors[ImGuiCol_TabActive] = ImVec4(0.34f, 0.24f, 0.15f, 1.0f);
  colors[ImGuiCol_TableHeaderBg] = ImVec4(0.22f, 0.29f, 0.16f, 1.0f);
  colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.20f, 0.14f, 0.10f, 0.35f);

  if (!high_contrast) {
    return;
  }

  colors[ImGuiCol_WindowBg] = ImVec4(0.03f, 0.03f, 0.06f, 0.96f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.07f, 0.10f, 0.92f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.16f, 1.0f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.28f, 0.44f, 1.0f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.32f, 0.54f, 1.0f);
  colors[ImGuiCol_Button] = ImVec4(0.16f, 0.26f, 0.42f, 1.0f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.38f, 0.62f, 1.0f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.44f, 0.72f, 1.0f);
  colors[ImGuiCol_Header] = ImVec4(0.16f, 0.28f, 0.46f, 1.0f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.42f, 0.66f, 1.0f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.32f, 0.50f, 0.78f, 1.0f);
  colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

void ShroomImGui_SetNextWindowPos(float x, float y, int condition) {
  ImGui::SetNextWindowPos(ImVec2(x, y), ToImGuiCond(condition));
}

void ShroomImGui_SetNextWindowSize(float width, float height, int condition) {
  ImGui::SetNextWindowSize(ImVec2(width, height), ToImGuiCond(condition));
}

void ShroomImGui_SetNextWindowBgAlpha(float alpha) { ImGui::SetNextWindowBgAlpha(alpha); }

bool ShroomImGui_Begin(const char* name, bool* open, int flags) {
  return ImGui::Begin(name, open, ToImGuiWindowFlags(flags));
}

void ShroomImGui_End(void) { ImGui::End(); }

void ShroomImGui_SetNextItemWidth(float width) { ImGui::SetNextItemWidth(width); }

void ShroomImGui_Text(const char* text) { ImGui::TextUnformatted(text); }

void ShroomImGui_TextWrapped(const char* text) { ImGui::TextWrapped("%s", text); }

void ShroomImGui_TextColored(ShroomImGuiColor color, const char* text) {
  ImGui::TextColored(ImVec4(color.r, color.g, color.b, color.a), "%s", text);
}

void ShroomImGui_TextDisabled(const char* text) { ImGui::TextDisabled("%s", text); }

void ShroomImGui_Separator(void) { ImGui::Separator(); }

void ShroomImGui_Spacing(void) { ImGui::Spacing(); }

void ShroomImGui_SameLine(void) { ImGui::SameLine(); }

bool ShroomImGui_Button(const char* label, float width, float height) {
  return ImGui::Button(label, ImVec2(width, height));
}

bool ShroomImGui_Checkbox(const char* label, bool* value) { return ImGui::Checkbox(label, value); }

bool ShroomImGui_SliderInt(const char* label, int* value, int minimum, int maximum,
                           const char* format) {
  return ImGui::SliderInt(label, value, minimum, maximum, format);
}

bool ShroomImGui_Combo(const char* label, int* current_item, const char* const items[],
                       int items_count) {
  bool changed = false;
  const char* preview_value;

  if ((current_item == NULL) || (items == NULL) || (items_count <= 0)) {
    return false;
  }

  preview_value = (*current_item >= 0) && (*current_item < items_count) ? items[*current_item] : "";
  if (!ImGui::BeginCombo(label, preview_value)) {
    return false;
  }

  for (int index = 0; index < items_count; ++index) {
    const bool selected = index == *current_item;

    if (ImGui::Selectable(items[index], selected)) {
      *current_item = index;
      changed = true;
    }
    if (selected) {
      ImGui::SetItemDefaultFocus();
    }
  }

  ImGui::EndCombo();
  return changed;
}

bool ShroomImGui_InputText(const char* label, char* buffer, size_t buffer_size) {
  return ImGui::InputText(label, buffer, buffer_size);
}

bool ShroomImGui_BeginChild(const char* id, float width, float height, bool border) {
  return ImGui::BeginChild(id, ImVec2(width, height), border);
}

void ShroomImGui_EndChild(void) { ImGui::EndChild(); }

void ShroomImGui_SetScrollHereY(float center_y_ratio) { ImGui::SetScrollHereY(center_y_ratio); }

bool ShroomImGui_WantCaptureKeyboard(void) { return ImGui::GetIO().WantCaptureKeyboard; }

void ShroomImGui_SetKeyboardFocusHere(void) { ImGui::SetKeyboardFocusHere(); }

void ShroomImGui_PushWindowRounding(float rounding) {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);
}

void ShroomImGui_PushWindowPadding(float x, float y) {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(x, y));
}

void ShroomImGui_PopStyleVar(void) { ImGui::PopStyleVar(); }

void ShroomImGui_PushStyleColor(int col, float r, float g, float b, float a) {
  ImGui::PushStyleColor((ImGuiCol)col, ImVec4(r, g, b, a));
}

void ShroomImGui_PopStyleColor(void) { ImGui::PopStyleColor(); }

bool ShroomImGui_InputTextWithSubmit(const char* label, char* buffer, size_t buffer_size,
                                     const char* submit_label) {
  bool submitted = false;
  ImGui::SetNextItemWidth(-80.0f);
  if (ImGui::InputText(label, buffer, buffer_size, ImGuiInputTextFlags_EnterReturnsTrue)) {
    submitted = true;
  }
  ImGui::SameLine();
  if (ImGui::Button(submit_label != NULL ? submit_label : "Send", ImVec2(72.0f, 0.0f))) {
    submitted = true;
  }
  return submitted;
}

bool ShroomImGui_BeginTable(const char* id, int columns, int flags, float width, float height) {
  return ImGui::BeginTable(id, columns, ToImGuiTableFlags(flags), ImVec2(width, height));
}

void ShroomImGui_EndTable(void) { ImGui::EndTable(); }

void ShroomImGui_TableSetupColumn(const char* label, float width) {
  ImGui::TableSetupColumn(label, ImGuiTableColumnFlags_None, width);
}

void ShroomImGui_TableHeadersRow(void) { ImGui::TableHeadersRow(); }

void ShroomImGui_TableNextRow(void) { ImGui::TableNextRow(); }

void ShroomImGui_TableSetColumnIndex(int index) { ImGui::TableSetColumnIndex(index); }

bool ShroomImGui_Selectable(const char* label, bool selected, int flags, float width,
                            float height) {
  return ImGui::Selectable(label, selected, ToImGuiSelectableFlags(flags), ImVec2(width, height));
}

} // extern "C"
