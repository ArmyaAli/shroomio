#include "imgui_impl_raylib.h"

#include "raylib.h"
#include "rlgl.h"

static double g_Time = 0.0;
static Texture2D g_FontTexture = {0};

static void UpdateMouseState(void) {
  ImGuiIO& io = ImGui::GetIO();

  io.AddMousePosEvent((float)GetMouseX(), (float)GetMouseY());

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  } else if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
    io.AddMouseButtonEvent(ImGuiMouseButton_Middle, true);
  } else if (IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) {
    io.AddMouseButtonEvent(ImGuiMouseButton_Middle, false);
  }

  {
    const Vector2 wheel = GetMouseWheelMoveV();
    io.AddMouseWheelEvent(wheel.x, wheel.y);
  }
}

static void UpdateKeyboardState(void) {
  ImGuiIO& io = ImGui::GetIO();

  io.AddKeyEvent(ImGuiMod_Ctrl, IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
  io.AddKeyEvent(ImGuiMod_Shift, IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
  io.AddKeyEvent(ImGuiMod_Alt, IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT));
  io.AddKeyEvent(ImGuiMod_Super, IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER));

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
  io.AddKeyEvent(ImGuiKey_B, IsKeyDown(KEY_B));
  io.AddKeyEvent(ImGuiKey_C, IsKeyDown(KEY_C));
  io.AddKeyEvent(ImGuiKey_D, IsKeyDown(KEY_D));
  io.AddKeyEvent(ImGuiKey_E, IsKeyDown(KEY_E));
  io.AddKeyEvent(ImGuiKey_F, IsKeyDown(KEY_F));
  io.AddKeyEvent(ImGuiKey_M, IsKeyDown(KEY_M));
  io.AddKeyEvent(ImGuiKey_R, IsKeyDown(KEY_R));
  io.AddKeyEvent(ImGuiKey_S, IsKeyDown(KEY_S));
  io.AddKeyEvent(ImGuiKey_V, IsKeyDown(KEY_V));
  io.AddKeyEvent(ImGuiKey_W, IsKeyDown(KEY_W));
  io.AddKeyEvent(ImGuiKey_X, IsKeyDown(KEY_X));
  io.AddKeyEvent(ImGuiKey_Y, IsKeyDown(KEY_Y));
  io.AddKeyEvent(ImGuiKey_Z, IsKeyDown(KEY_Z));

  for (int character = GetCharPressed(); character > 0; character = GetCharPressed()) {
    io.AddInputCharacter((unsigned int)character);
  }
}

static void EnableScissor(float x, float y, float width, float height) {
  const ImGuiIO& io = ImGui::GetIO();

  rlEnableScissorTest();
  rlScissor((int)x, (int)(io.DisplaySize.y - (y + height)), (int)width, (int)height);
}

static void RenderVertex(const ImDrawVert& vertex) {
  rlColor4ub((unsigned char)(vertex.col >> IM_COL32_R_SHIFT),
             (unsigned char)(vertex.col >> IM_COL32_G_SHIFT),
             (unsigned char)(vertex.col >> IM_COL32_B_SHIFT),
             (unsigned char)(vertex.col >> IM_COL32_A_SHIFT));
  rlTexCoord2f(vertex.uv.x, vertex.uv.y);
  rlVertex2f(vertex.pos.x, vertex.pos.y);
}

static void RenderTriangles(unsigned int count, int index_start,
                            const ImVector<ImDrawIdx>& index_buffer,
                            const ImVector<ImDrawVert>& vertex_buffer, ImTextureID texture_id) {
  if (count < 3) {
    return;
  }

  rlBegin(RL_TRIANGLES);
  rlSetTexture((unsigned int)texture_id);

  for (unsigned int index = 0; index <= (count - 3); index += 3) {
    const ImDrawIdx index_a = index_buffer[index_start + (int)index];
    const ImDrawIdx index_b = index_buffer[index_start + (int)index + 1];
    const ImDrawIdx index_c = index_buffer[index_start + (int)index + 2];

    RenderVertex(vertex_buffer[index_a]);
    RenderVertex(vertex_buffer[index_b]);
    RenderVertex(vertex_buffer[index_c]);
  }

  rlEnd();
}

bool ImGui_ImplRaylib_Init(void) {
  ImGuiIO& io = ImGui::GetIO();
  unsigned char* pixels = NULL;
  int width = 0;
  int height = 0;
  Image image = {0};

  io.BackendPlatformName = "imgui_impl_raylib";
  io.BackendRendererName = "imgui_impl_raylib";

  io.Fonts->AddFontDefault();
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  image.data = pixels;
  image.width = width;
  image.height = height;
  image.mipmaps = 1;
  image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

  g_FontTexture = LoadTextureFromImage(image);
  SetTextureFilter(g_FontTexture, TEXTURE_FILTER_BILINEAR);
  io.Fonts->TexID = (ImTextureID)(unsigned long long)g_FontTexture.id;

  g_Time = 0.0;
  return true;
}

void ImGui_ImplRaylib_Shutdown(void) {
  ImFontAtlas* fonts = ImGui::GetIO().Fonts;

  fonts->TexID = (ImTextureID)0;
  if (g_FontTexture.id != 0) {
    UnloadTexture(g_FontTexture);
    g_FontTexture.id = 0;
  }
  g_Time = 0.0;
}

void ImGui_ImplRaylib_NewFrame(void) {
  ImGuiIO& io = ImGui::GetIO();
  const double current_time = GetTime();

  io.DisplaySize = ImVec2((float)GetScreenWidth(), (float)GetScreenHeight());
  io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
  io.DeltaTime = g_Time > 0.0 ? (float)(current_time - g_Time) : (1.0f / 60.0f);
  g_Time = current_time;

  UpdateMouseState();
  UpdateKeyboardState();
}

void ImGui_ImplRaylib_RenderDrawData(ImDrawData* draw_data) {
  if ((draw_data == NULL) || (draw_data->CmdListsCount <= 0)) {
    return;
  }

  rlDrawRenderBatchActive();
  rlDisableBackfaceCulling();

  for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index) {
    const ImDrawList* command_list = draw_data->CmdLists[list_index];

    for (int command_index = 0; command_index < command_list->CmdBuffer.Size; ++command_index) {
      const ImDrawCmd* command = &command_list->CmdBuffer[command_index];

      EnableScissor(command->ClipRect.x - draw_data->DisplayPos.x,
                    command->ClipRect.y - draw_data->DisplayPos.y,
                    command->ClipRect.z - (command->ClipRect.x - draw_data->DisplayPos.x),
                    command->ClipRect.w - (command->ClipRect.y - draw_data->DisplayPos.y));

      if (command->UserCallback != NULL) {
        command->UserCallback(command_list, command);
        continue;
      }

      RenderTriangles(command->ElemCount, command->IdxOffset, command_list->IdxBuffer,
                      command_list->VtxBuffer, command->GetTexID());
      rlDrawRenderBatchActive();
    }
  }

  rlSetTexture(0);
  rlDisableScissorTest();
  rlEnableBackfaceCulling();
}
