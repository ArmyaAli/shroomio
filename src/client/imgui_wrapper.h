#ifndef SHROOM_IMGUI_WRAPPER_H
#define SHROOM_IMGUI_WRAPPER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ShroomImGui_Init(void);
void ShroomImGui_Shutdown(void);
void ShroomImGui_NewFrame(void);
void ShroomImGui_Render(void);
void ShroomImGui_ShowDemoWindow(bool* p_open);

#ifdef __cplusplus
}
#endif

#endif // SHROOM_IMGUI_WRAPPER_H
