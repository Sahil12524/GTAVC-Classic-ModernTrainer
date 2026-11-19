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
void ImGui_ImplDX8_InvalidateFontTexture();   // fonts changed: rebuilt on the next NewFrame
void ImGui_ImplDX8_WrapTexturesCallback(const ImDrawList*, const ImDrawCmd*);   // textures repeat until ImDrawCallback_ResetRenderState

// 3D mesh with the depth buffer inside a screen rectangle (vehicle preview).
struct ImGuiDX8MeshVertex { float x, y, z; unsigned col; float u, v; };   // screen pixels, z = depth 0..1, col = D3DCOLOR
struct ImGuiDX8MeshBatch { void* texture; int first, count; bool seeThrough; };   // texture: IDirect3DTexture8* or null
void ImGui_ImplDX8_DrawDepthMesh(float x0, float y0, float x1, float y1,
                                 const ImGuiDX8MeshVertex* verts, const ImGuiDX8MeshBatch* batches, int numBatches);
// -> managed IDirect3DTexture8*, or null; *retryLater = the device couldn't do it right now (being reset)
void* ImGui_ImplDX8_RenderMeshToTexture(int size, const ImGuiDX8MeshVertex* verts,
                                        const ImGuiDX8MeshBatch* batches, int numBatches, bool* retryLater);
