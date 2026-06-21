#include "layout.h"

#include "imgui_wrapper.h"
#include "raylib.h"

bool ShroomLayoutBeginCenteredPanel(const char* title, float width, float height, float alpha,
                                    int flags) {
  ShroomImGui_SetNextWindowPos(((float)GetScreenWidth() - width) * 0.5f,
                               ((float)GetScreenHeight() - height) * 0.5f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(width, height, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(alpha);
  return ShroomImGui_Begin(title, NULL, flags);
}

bool ShroomLayoutButtonFullWidth(const char* label, float height) {
  return ShroomImGui_Button(label, -1.0f, height);
}

void ShroomLayoutHeading(const char* text) {
  ShroomImGui_Text(text);
  ShroomImGui_Separator();
  ShroomImGui_Spacing();
}
