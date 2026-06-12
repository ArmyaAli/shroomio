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

  ImGui::StyleColorsDark();
  style.FrameRounding = 6.0f;
  style.WindowRounding = 10.0f;
  style.GrabRounding = 6.0f;

  if (!high_contrast) {
    return;
  }

  ImVec4* colors = style.Colors;
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
