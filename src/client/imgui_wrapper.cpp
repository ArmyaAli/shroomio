#include "imgui_wrapper.h"

#include "imgui.h"
#include "imgui_impl_raylib.h"

extern "C" {

void ShroomImGui_Init(void) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
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

void ShroomImGui_ShowDemoWindow(bool* p_open) {
    ImGui::ShowDemoWindow(p_open);
}

} // extern "C"
