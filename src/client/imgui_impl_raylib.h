#ifndef SHROOM_IMGUI_IMPL_RAYLIB_H
#define SHROOM_IMGUI_IMPL_RAYLIB_H

#include "imgui.h"

IMGUI_IMPL_API bool ImGui_ImplRaylib_Init(void);
IMGUI_IMPL_API void ImGui_ImplRaylib_Shutdown(void);
IMGUI_IMPL_API void ImGui_ImplRaylib_NewFrame(void);
IMGUI_IMPL_API void ImGui_ImplRaylib_RenderDrawData(ImDrawData* draw_data);

#endif
