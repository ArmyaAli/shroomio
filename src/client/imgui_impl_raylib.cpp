#include "imgui_impl_raylib.h"

#include "raylib.h"
#include "raymath.h"

#include <cstring>

static double g_Time = 0.0f;
static bool g_MouseJustPressed[5] = {false, false, false, false, false};
static ImGuiMouseCursor g_LastMouseCursor = ImGuiMouseCursor_COUNT;

static void ImGui_ImplRaylib_UpdateMousePosAndButtons(void) {
    ImGuiIO& io = ImGui::GetIO();

    for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++) {
        io.MouseDown[i] = g_MouseJustPressed[i] || IsMouseButtonDown(i);
        g_MouseJustPressed[i] = false;
    }

    io.MousePos = ImVec2((float)GetMouseX(), (float)GetMouseY());
}

static void ImGui_ImplRaylib_UpdateMouseCursor(void) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return;

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor) {
        HideCursor();
    } else {
        ShowCursor();
        if (imgui_cursor != g_LastMouseCursor) {
            g_LastMouseCursor = imgui_cursor;
        }
    }
}

bool ImGui_ImplRaylib_Init(void) {
    ImGuiIO& io = ImGui::GetIO();

    io.BackendPlatformName = "imgui_impl_raylib";
    io.BackendRendererName = "imgui_impl_raylib";

    g_Time = 0.0f;

    return true;
}

void ImGui_ImplRaylib_Shutdown(void) {
    g_Time = 0.0f;
    g_LastMouseCursor = ImGuiMouseCursor_COUNT;
}

void ImGui_ImplRaylib_NewFrame(void) {
    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2((float)GetScreenWidth(), (float)GetScreenHeight());

    double current_time = GetTime();
    io.DeltaTime = g_Time > 0.0f ? (float)(current_time - g_Time) : (1.0f / 60.0f);
    g_Time = current_time;

    ImGui_ImplRaylib_UpdateMousePosAndButtons();
    ImGui_ImplRaylib_UpdateMouseCursor();

    io.KeyCtrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    io.KeyShift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    io.KeyAlt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    io.KeySuper = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);

    io.AddKeyEvent(ImGuiKey_Tab, IsKeyDown(KEY_TAB));
    io.AddKeyEvent(ImGuiKey_LeftArrow, IsKeyDown(KEY_LEFT));
    io.AddKeyEvent(ImGuiKey_RightArrow, IsKeyDown(KEY_RIGHT));
    io.AddKeyEvent(ImGuiKey_UpArrow, IsKeyDown(KEY_UP));
    io.AddKeyEvent(ImGuiKey_DownArrow, IsKeyDown(KEY_DOWN));
    io.AddKeyEvent(ImGuiKey_PageUp, IsKeyDown(KEY_PAGE_UP));
    io.AddKeyEvent(ImGuiKey_PageDown, IsKeyDown(KEY_PAGE_DOWN));
    io.AddKeyEvent(ImGuiKey_Home, IsKeyDown(KEY_HOME));
    io.AddKeyEvent(ImGuiKey_End, IsKeyDown(KEY_END));
    io.AddKeyEvent(ImGuiKey_Insert, IsKeyDown(KEY_INSERT));
    io.AddKeyEvent(ImGuiKey_Delete, IsKeyDown(KEY_DELETE));
    io.AddKeyEvent(ImGuiKey_Backspace, IsKeyDown(KEY_BACKSPACE));
    io.AddKeyEvent(ImGuiKey_Space, IsKeyDown(KEY_SPACE));
    io.AddKeyEvent(ImGuiKey_Enter, IsKeyDown(KEY_ENTER));
    io.AddKeyEvent(ImGuiKey_Escape, IsKeyDown(KEY_ESCAPE));
    io.AddKeyEvent(ImGuiKey_A, IsKeyDown(KEY_A));
    io.AddKeyEvent(ImGuiKey_C, IsKeyDown(KEY_C));
    io.AddKeyEvent(ImGuiKey_V, IsKeyDown(KEY_V));
    io.AddKeyEvent(ImGuiKey_X, IsKeyDown(KEY_X));
    io.AddKeyEvent(ImGuiKey_Y, IsKeyDown(KEY_Y));
    io.AddKeyEvent(ImGuiKey_Z, IsKeyDown(KEY_Z));

    if (IsKeyPressed(KEY_BACKSPACE)) {
        io.AddInputCharacter('\b');
    }

    int key = GetCharPressed();
    while (key > 0) {
        io.AddInputCharacter((unsigned int)key);
        key = GetCharPressed();
    }
}

static void ImGui_ImplRaylib_RenderTriangles(ImDrawVert* vtx_buf, ImDrawIdx* idx_buf, int count) {
    for (int i = 0; i < count; i += 3) {
        ImDrawVert& v0 = vtx_buf[idx_buf[i + 0]];
        ImDrawVert& v1 = vtx_buf[idx_buf[i + 1]];
        ImDrawVert& v2 = vtx_buf[idx_buf[i + 2]];

        Color c0 = {(unsigned char)(v0.col >> 0), (unsigned char)(v0.col >> 8),
                    (unsigned char)(v0.col >> 16), (unsigned char)(v0.col >> 24)};
        Color c1 = {(unsigned char)(v1.col >> 0), (unsigned char)(v1.col >> 8),
                    (unsigned char)(v1.col >> 16), (unsigned char)(v1.col >> 24)};
        Color c2 = {(unsigned char)(v2.col >> 0), (unsigned char)(v2.col >> 8),
                    (unsigned char)(v2.col >> 16), (unsigned char)(v2.col >> 24)};

        DrawTriangleLines((Vector2){v0.pos.x, v0.pos.y}, (Vector2){v1.pos.x, v1.pos.y},
                          (Vector2){v2.pos.x, v2.pos.y}, c0);
    }
}

void ImGui_ImplRaylib_RenderDrawData(ImDrawData* draw_data) {
    if (draw_data->CmdListsCount == 0)
        return;

    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;
        const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cmd_list, pcmd);
            } else {
                ImVec2 clip_min((float)pcmd->ClipRect.x, (float)pcmd->ClipRect.y);
                ImVec2 clip_max((float)pcmd->ClipRect.z, (float)pcmd->ClipRect.w);

                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                BeginScissorMode((int)clip_min.x, (int)clip_min.y,
                                 (int)(clip_max.x - clip_min.x), (int)(clip_max.y - clip_min.y));

                for (unsigned int i = 0; i < pcmd->ElemCount; i += 3) {
                    const ImDrawVert& v0 = vtx_buffer[idx_buffer[pcmd->IdxOffset + i + 0]];
                    const ImDrawVert& v1 = vtx_buffer[idx_buffer[pcmd->IdxOffset + i + 1]];
                    const ImDrawVert& v2 = vtx_buffer[idx_buffer[pcmd->IdxOffset + i + 2]];

                    Color c = {(unsigned char)(v0.col >> 0), (unsigned char)(v0.col >> 8),
                               (unsigned char)(v0.col >> 16), (unsigned char)(v0.col >> 24)};

                    DrawTriangle((Vector2){v0.pos.x, v0.pos.y}, (Vector2){v1.pos.x, v1.pos.y},
                                 (Vector2){v2.pos.x, v2.pos.y}, c);
                }

                EndScissorMode();
            }
        }
    }
}
