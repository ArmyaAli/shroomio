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

void ShroomImGui_SetUiScale(float scale) {
  const float clamped_scale = scale < 0.8f ? 0.8f : (scale > 1.6f ? 1.6f : scale);
  ImGui::GetIO().FontGlobalScale = clamped_scale;
  ImGui::GetStyle().ScaleAllSizes(clamped_scale);
}

void ShroomImGui_ApplyTheme(bool high_contrast) {
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors;

  style = ImGuiStyle();

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

  if (!high_contrast) {
    // Fungal palette - organic, earthy tones matching the art style
    // Base colors from Cartoony-Art-Style.md
    const ImVec4 night_mycelium = ImVec4(0.071f, 0.071f, 0.102f, 1.0f);      // #12121A
    const ImVec4 deep_cap_brown = ImVec4(0.243f, 0.165f, 0.129f, 1.0f);     // #3E2A21
    const ImVec4 spore_gold = ImVec4(1.0f, 0.894f, 0.439f, 1.0f);           // #FFE470
    const ImVec4 moss_green = ImVec4(0.439f, 0.878f, 0.502f, 1.0f);         // #70E080
    const ImVec4 glow_purple = ImVec4(0.729f, 0.408f, 0.784f, 1.0f);        // #BA68C8

    // Text
    colors[ImGuiCol_Text] = ImVec4(0.96f, 0.92f, 0.82f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.58f, 0.52f, 0.44f, 1.0f);

    // Backgrounds - use night mycelium as base
    colors[ImGuiCol_WindowBg] = ImVec4(night_mycelium.x, night_mycelium.y, night_mycelium.z, 0.94f);
    colors[ImGuiCol_ChildBg] = ImVec4(night_mycelium.x * 1.2f, night_mycelium.y * 1.2f, night_mycelium.z * 1.2f, 0.90f);
    colors[ImGuiCol_PopupBg] = ImVec4(night_mycelium.x * 1.1f, night_mycelium.y * 1.1f, night_mycelium.z * 1.1f, 0.96f);

    // Borders - deep cap brown
    colors[ImGuiCol_Border] = ImVec4(deep_cap_brown.x, deep_cap_brown.y, deep_cap_brown.z, 0.58f);
    colors[ImGuiCol_Separator] = ImVec4(deep_cap_brown.x, deep_cap_brown.y, deep_cap_brown.z, 0.52f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(deep_cap_brown.x * 1.3f, deep_cap_brown.y * 1.3f, deep_cap_brown.z * 1.3f, 0.86f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(deep_cap_brown.x * 1.6f, deep_cap_brown.y * 1.6f, deep_cap_brown.z * 1.6f, 1.0f);

    // Frames - warm stem tan variants
    colors[ImGuiCol_FrameBg] = ImVec4(deep_cap_brown.x * 1.2f, deep_cap_brown.y * 1.2f, deep_cap_brown.z * 1.2f, 0.92f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(deep_cap_brown.x * 1.8f, deep_cap_brown.y * 1.8f, deep_cap_brown.z * 1.8f, 0.96f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(deep_cap_brown.x * 2.2f, deep_cap_brown.y * 2.2f, deep_cap_brown.z * 2.2f, 1.0f);

    // Titles
    colors[ImGuiCol_TitleBg] = ImVec4(night_mycelium.x * 1.5f, night_mycelium.y * 1.5f, night_mycelium.z * 1.5f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(deep_cap_brown.x, deep_cap_brown.y, deep_cap_brown.z, 1.0f);

    // Buttons - warm stem tan to spore gold gradient
    colors[ImGuiCol_Button] = ImVec4(0.49f, 0.32f, 0.20f, 0.96f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.72f, 0.50f, 0.28f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(spore_gold.x * 0.9f, spore_gold.y * 0.9f, spore_gold.z * 0.9f, 1.0f);

    // Headers - moss green for positive actions
    colors[ImGuiCol_Header] = ImVec4(moss_green.x * 0.5f, moss_green.y * 0.5f, moss_green.z * 0.5f, 0.86f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(moss_green.x * 0.7f, moss_green.y * 0.7f, moss_green.z * 0.7f, 0.96f);
    colors[ImGuiCol_HeaderActive] = ImVec4(moss_green.x * 0.85f, moss_green.y * 0.85f, moss_green.z * 0.85f, 1.0f);

    // Interactive elements
    colors[ImGuiCol_CheckMark] = spore_gold;
    colors[ImGuiCol_SliderGrab] = ImVec4(spore_gold.x * 0.9f, spore_gold.y * 0.9f, spore_gold.z * 0.9f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = spore_gold;

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(night_mycelium.x * 0.8f, night_mycelium.y * 0.8f, night_mycelium.z * 0.8f, 0.86f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(deep_cap_brown.x * 1.5f, deep_cap_brown.y * 1.5f, deep_cap_brown.z * 1.5f, 0.94f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(deep_cap_brown.x * 2.0f, deep_cap_brown.y * 2.0f, deep_cap_brown.z * 2.0f, 0.98f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(deep_cap_brown.x * 2.5f, deep_cap_brown.y * 2.5f, deep_cap_brown.z * 2.5f, 1.0f);

    // Resize grip
    colors[ImGuiCol_ResizeGrip] = ImVec4(deep_cap_brown.x * 1.8f, deep_cap_brown.y * 1.8f, deep_cap_brown.z * 1.8f, 0.40f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(deep_cap_brown.x * 2.5f, deep_cap_brown.y * 2.5f, deep_cap_brown.z * 2.5f, 0.68f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(spore_gold.x * 0.8f, spore_gold.y * 0.8f, spore_gold.z * 0.8f, 0.90f);

    // Navigation and plots
    colors[ImGuiCol_NavHighlight] = glow_purple;
    colors[ImGuiCol_PlotLines] = ImVec4(deep_cap_brown.x * 2.5f, deep_cap_brown.y * 2.5f, deep_cap_brown.z * 2.5f, 0.86f);
    colors[ImGuiCol_PlotLinesHovered] = spore_gold;
    colors[ImGuiCol_PlotHistogram] = moss_green;
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(moss_green.x * 1.2f, moss_green.y * 1.2f, moss_green.z * 1.2f, 1.0f);

    // Drag and drop
    colors[ImGuiCol_DragDropTarget] = ImVec4(spore_gold.x * 0.9f, spore_gold.y * 0.9f, spore_gold.z * 0.9f, 0.90f);

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(deep_cap_brown.x * 0.9f, deep_cap_brown.y * 0.9f, deep_cap_brown.z * 0.9f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(deep_cap_brown.x * 1.8f, deep_cap_brown.y * 1.8f, deep_cap_brown.z * 1.8f, 1.0f);
    colors[ImGuiCol_TabActive] = ImVec4(deep_cap_brown.x * 1.4f, deep_cap_brown.y * 1.4f, deep_cap_brown.z * 1.4f, 1.0f);

    // Tables - fungal palette
    colors[ImGuiCol_TableHeaderBg] = ImVec4(moss_green.x * 0.4f, moss_green.y * 0.4f, moss_green.z * 0.4f, 1.0f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(deep_cap_brown.x * 0.8f, deep_cap_brown.y * 0.8f, deep_cap_brown.z * 0.8f, 0.35f);
  } else {
    // High contrast - brighter, more saturated fungal colors with stronger contrast
    const ImVec4 night_mycelium_dark = ImVec4(0.03f, 0.03f, 0.06f, 1.0f);
    const ImVec4 deep_cap_brown_dark = ImVec4(0.15f, 0.10f, 0.08f, 1.0f);
    const ImVec4 spore_gold_bright = ImVec4(1.0f, 0.95f, 0.55f, 1.0f);
    const ImVec4 moss_green_bright = ImVec4(0.55f, 0.95f, 0.60f, 1.0f);

    // Text - pure white for maximum contrast
    colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

    // Backgrounds - darker for contrast
    colors[ImGuiCol_WindowBg] = ImVec4(night_mycelium_dark.x, night_mycelium_dark.y, night_mycelium_dark.z, 0.96f);
    colors[ImGuiCol_ChildBg] = ImVec4(night_mycelium_dark.x * 1.5f, night_mycelium_dark.y * 1.5f, night_mycelium_dark.z * 1.5f, 0.92f);
    colors[ImGuiCol_PopupBg] = ImVec4(night_mycelium_dark.x * 1.3f, night_mycelium_dark.y * 1.3f, night_mycelium_dark.z * 1.3f, 0.96f);

    // Borders - brighter
    colors[ImGuiCol_Border] = ImVec4(deep_cap_brown_dark.x * 2.0f, deep_cap_brown_dark.y * 2.0f, deep_cap_brown_dark.z * 2.0f, 0.70f);
    colors[ImGuiCol_Separator] = ImVec4(deep_cap_brown_dark.x * 2.0f, deep_cap_brown_dark.y * 2.0f, deep_cap_brown_dark.z * 2.0f, 0.60f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(deep_cap_brown_dark.x * 3.0f, deep_cap_brown_dark.y * 3.0f, deep_cap_brown_dark.z * 3.0f, 0.90f);
    colors[ImGuiCol_SeparatorActive] = spore_gold_bright;

    // Frames
    colors[ImGuiCol_FrameBg] = ImVec4(deep_cap_brown_dark.x * 1.5f, deep_cap_brown_dark.y * 1.5f, deep_cap_brown_dark.z * 1.5f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(deep_cap_brown_dark.x * 2.5f, deep_cap_brown_dark.y * 2.5f, deep_cap_brown_dark.z * 2.5f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(deep_cap_brown_dark.x * 3.5f, deep_cap_brown_dark.y * 3.5f, deep_cap_brown_dark.z * 3.5f, 1.0f);

    // Titles
    colors[ImGuiCol_TitleBg] = ImVec4(deep_cap_brown_dark.x, deep_cap_brown_dark.y, deep_cap_brown_dark.z, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(deep_cap_brown_dark.x * 2.0f, deep_cap_brown_dark.y * 2.0f, deep_cap_brown_dark.z * 2.0f, 1.0f);

    // Buttons - high contrast warm tones
    colors[ImGuiCol_Button] = ImVec4(0.35f, 0.22f, 0.14f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.55f, 0.38f, 0.22f, 1.0f);
    colors[ImGuiCol_ButtonActive] = spore_gold_bright;

    // Headers - bright moss green
    colors[ImGuiCol_Header] = moss_green_bright;
    colors[ImGuiCol_HeaderHovered] = ImVec4(moss_green_bright.x * 1.1f, moss_green_bright.y * 1.1f, moss_green_bright.z * 1.1f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(moss_green_bright.x * 1.2f, moss_green_bright.y * 1.2f, moss_green_bright.z * 1.2f, 1.0f);

    // Interactive elements
    colors[ImGuiCol_CheckMark] = spore_gold_bright;
    colors[ImGuiCol_SliderGrab] = spore_gold_bright;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 1.0f, 0.7f, 1.0f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(night_mycelium_dark.x * 0.8f, night_mycelium_dark.y * 0.8f, night_mycelium_dark.z * 0.8f, 0.90f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(deep_cap_brown_dark.x * 2.5f, deep_cap_brown_dark.y * 2.5f, deep_cap_brown_dark.z * 2.5f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(deep_cap_brown_dark.x * 3.5f, deep_cap_brown_dark.y * 3.5f, deep_cap_brown_dark.z * 3.5f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = spore_gold_bright;

    // Resize grip
    colors[ImGuiCol_ResizeGrip] = ImVec4(deep_cap_brown_dark.x * 2.0f, deep_cap_brown_dark.y * 2.0f, deep_cap_brown_dark.z * 2.0f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(deep_cap_brown_dark.x * 3.0f, deep_cap_brown_dark.y * 3.0f, deep_cap_brown_dark.z * 3.0f, 0.80f);
    colors[ImGuiCol_ResizeGripActive] = spore_gold_bright;

    // Navigation and plots
    colors[ImGuiCol_NavHighlight] = ImVec4(1.0f, 0.6f, 0.9f, 1.0f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.95f, 0.80f, 0.60f, 1.0f);
    colors[ImGuiCol_PlotLinesHovered] = spore_gold_bright;
    colors[ImGuiCol_PlotHistogram] = moss_green_bright;
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(moss_green_bright.x * 1.2f, moss_green_bright.y * 1.2f, moss_green_bright.z * 1.2f, 1.0f);

    // Drag and drop
    colors[ImGuiCol_DragDropTarget] = spore_gold_bright;

    // Tabs
    colors[ImGuiCol_Tab] = deep_cap_brown_dark;
    colors[ImGuiCol_TabHovered] = ImVec4(deep_cap_brown_dark.x * 2.5f, deep_cap_brown_dark.y * 2.5f, deep_cap_brown_dark.z * 2.5f, 1.0f);
    colors[ImGuiCol_TabActive] = ImVec4(deep_cap_brown_dark.x * 1.8f, deep_cap_brown_dark.y * 1.8f, deep_cap_brown_dark.z * 1.8f, 1.0f);

    // Tables - high contrast
    colors[ImGuiCol_TableHeaderBg] = moss_green_bright;
    colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(deep_cap_brown_dark.x * 1.2f, deep_cap_brown_dark.y * 1.2f, deep_cap_brown_dark.z * 1.2f, 0.45f);
  }
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

float ShroomImGui_GetContentRegionAvailWidth(void) { return ImGui::GetContentRegionAvail().x; }

float ShroomImGui_GetContentRegionAvailHeight(void) { return ImGui::GetContentRegionAvail().y; }

float ShroomImGui_CalcTextWidth(const char* text) {
  return text != nullptr ? ImGui::CalcTextSize(text).x : 0.0f;
}

float ShroomImGui_CalcWrappedTextHeight(const char* text, float wrap_width) {
  return text != nullptr ? ImGui::CalcTextSize(text, nullptr, false, wrap_width).y : 0.0f;
}

float ShroomImGui_GetItemInnerSpacingX(void) { return ImGui::GetStyle().ItemInnerSpacing.x; }

void ShroomImGui_Text(const char* text) { ImGui::TextUnformatted(text); }

void ShroomImGui_TextWrapped(const char* text) { ImGui::TextWrapped("%s", text); }

void ShroomImGui_TextColored(ShroomImGuiColor color, const char* text) {
  ImGui::TextColored(ImVec4(color.r, color.g, color.b, color.a), "%s", text);
}

void ShroomImGui_TextColoredWrapped(ShroomImGuiColor color, const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color.r, color.g, color.b, color.a));
  ImGui::TextWrapped("%s", text);
  ImGui::PopStyleColor();
}

void ShroomImGui_TextDisabled(const char* text) { ImGui::TextDisabled("%s", text); }

void ShroomImGui_TextDisabledWrapped(const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextWrapped("%s", text);
  ImGui::PopStyleColor();
}

void ShroomImGui_Separator(void) { ImGui::Separator(); }

void ShroomImGui_Spacing(void) { ImGui::Spacing(); }

void ShroomImGui_SameLine(void) { ImGui::SameLine(); }

bool ShroomImGui_Button(const char* label, float width, float height) {
  return ImGui::Button(label, ImVec2(width, height));
}

bool ShroomImGui_IsLastItemVisible(void) { return ImGui::IsItemVisible(); }

void ShroomImGui_BeginDisabled(bool disabled) { ImGui::BeginDisabled(disabled); }

void ShroomImGui_EndDisabled(void) { ImGui::EndDisabled(); }

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

void ShroomImGui_TableSetupScrollFreeze(int columns, int rows) {
  ImGui::TableSetupScrollFreeze(columns, rows);
}

void ShroomImGui_TableHeadersRow(void) { ImGui::TableHeadersRow(); }

void ShroomImGui_TableNextRow(void) { ImGui::TableNextRow(); }

void ShroomImGui_TableSetColumnIndex(int index) { ImGui::TableSetColumnIndex(index); }

bool ShroomImGui_Selectable(const char* label, bool selected, int flags, float width,
                            float height) {
  return ImGui::Selectable(label, selected, ToImGuiSelectableFlags(flags), ImVec2(width, height));
}

} // extern "C"
