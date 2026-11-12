// Minimal Dear ImGui renderer for Direct3D 8 (written for this trainer).
#pragma once
struct IDirect3DDevice8;

bool ImGui_ImplDX8_Init(IDirect3DDevice8* device);
void ImGui_ImplDX8_Shutdown();
void ImGui_ImplDX8_NewFrame();
void ImGui_ImplDX8_RenderDrawData(struct ImDrawData* draw_data);
// True if p looks like a live Direct3D 8 texture object of this device (used
// to show the game's own radar icons in the menu).
bool ImGui_ImplDX8_IsTexture(void* p);
