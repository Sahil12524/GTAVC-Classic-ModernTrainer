// Minimal Dear ImGui renderer for Direct3D 8.
//
// D3D8 has no scissor test, so each draw command is clipped by setting the
// viewport to its clip rectangle and building an orthographic projection that
// covers exactly that rectangle. Vertex colors already match D3DCOLOR because
// the project is compiled with IMGUI_USE_BGRA_PACKED_COLOR.
#include "imgui.h"
#include "imgui_impl_dx8.h"
#include "trainer.h"
#include <d3d8.h>
#include <vector>
#include <string.h>

struct Dx8Vertex { float x, y, z; D3DCOLOR col; float u, v; };
static const DWORD kFVF = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;

static IDirect3DDevice8*  g_dev = nullptr;
static IDirect3DTexture8* g_fontTex = nullptr;
static std::vector<Dx8Vertex> g_vtx;

static bool CreateFontTexture() {
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* px; int w, h;
    io.Fonts->GetTexDataAsRGBA32(&px, &w, &h);
    static int logs = 0;
    HRESULT hr = g_dev->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_fontTex);
    if (hr != D3D_OK) {
        g_fontTex = nullptr;
        if (logs++ < 5) Log("Menu font texture %dx%d not made (0x%08lX) - tried again next frame", w, h, (unsigned long)hr);
        return false;
    }
    D3DLOCKED_RECT lr;
    if ((hr = g_fontTex->LockRect(0, &lr, nullptr, 0)) != D3D_OK) {
        g_fontTex->Release(); g_fontTex = nullptr;                // retried next frame, not left blank
        if (logs++ < 5) Log("Menu font texture not filled (lock 0x%08lX) - tried again next frame", (unsigned long)hr);
        return false;
    }
    for (int y = 0; y < h; y++) {
        const unsigned char* src = px + y * w * 4;
        unsigned char* dst = (unsigned char*)lr.pBits + y * lr.Pitch;
        for (int x = 0; x < w; x++) { // RGBA -> BGRA
            dst[x * 4 + 0] = src[x * 4 + 2];
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4 + 0];
            dst[x * 4 + 3] = src[x * 4 + 3];
        }
    }
    g_fontTex->UnlockRect(0);
    io.Fonts->SetTexID((ImTextureID)(intptr_t)g_fontTex);
    return true;
}

bool ImGui_ImplDX8_Init(IDirect3DDevice8* device) {
    g_dev = device;
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "imgui_impl_dx8";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;   // big draw lists (the vehicle preview) go past 65535 vertices
    return true;
}

void ImGui_ImplDX8_Shutdown() {
    if (g_fontTex) { g_fontTex->Release(); g_fontTex = nullptr; }
    g_dev = nullptr;
}

void ImGui_ImplDX8_InvalidateFontTexture() {
    if (g_fontTex) { g_fontTex->Release(); g_fontTex = nullptr; }
}

void ImGui_ImplDX8_NewFrame() {
    if (!g_fontTex) CreateFontTexture();
}

static bool Readable(const void* p, size_t n) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!p || !VirtualQuery(p, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    return (const char*)p + n <= (const char*)mbi.BaseAddress + mbi.RegionSize;
}

// Only memory reads, no calls: p counts as a texture when it is the same kind
// of object as our own font texture (same vtable), i.e. a texture made by
// this device - whether that is the real d3d8 or a wrapper such as VCModernFix.
bool ImGui_ImplDX8_IsTexture(void* p) {
    if (!g_dev || !g_fontTex || !Readable(p, 4)) return false;
    return *(void**)p == *(void**)g_fontTex;
}

static void SetupRenderState() {
    g_dev->SetPixelShader(0);
    g_dev->SetVertexShader(kFVF);
    g_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    g_dev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    g_dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    g_dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    g_dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    g_dev->SetRenderState(D3DRS_CLIPPING, TRUE);
    g_dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    g_dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    g_dev->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    g_dev->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    g_dev->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
    g_dev->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    g_dev->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
    g_dev->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    g_dev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    g_dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    g_dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    D3DMATRIX ident = { { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } } };
    g_dev->SetTransform(D3DTS_WORLD, &ident);
    g_dev->SetTransform(D3DTS_VIEW, &ident);
}

// Viewport = clip rect, projection maps the clip rect onto the whole viewport.
static void SetClip(float x0, float y0, float x1, float y1) {
    D3DVIEWPORT8 vp;
    vp.X = (DWORD)x0; vp.Y = (DWORD)y0;
    vp.Width = (DWORD)(x1 - x0); vp.Height = (DWORD)(y1 - y0);
    vp.MinZ = 0.0f; vp.MaxZ = 1.0f;
    g_dev->SetViewport(&vp);

    float L = (float)vp.X + 0.5f, R = (float)(vp.X + vp.Width) + 0.5f;
    float T = (float)vp.Y + 0.5f, B = (float)(vp.Y + vp.Height) + 0.5f;
    D3DMATRIX proj = { { {
        2.0f / (R - L), 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / (T - B), 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        (L + R) / (L - R), (T + B) / (B - T), 0.5f, 1.0f } } };
    g_dev->SetTransform(D3DTS_PROJECTION, &proj);
}

void ImGui_ImplDX8_RenderDrawData(ImDrawData* dd) {
    if (!g_dev || dd->DisplaySize.x <= 0.0f || dd->DisplaySize.y <= 0.0f) return;

    DWORD saved = 0;
    if (HRESULT hr = g_dev->CreateStateBlock(D3DSBT_ALL, &saved); hr != D3D_OK) {
        static int logs = 0;                              // nothing drawn rather than the game's state left changed
        if (logs++ < 5) Log("Menu frame not drawn: CreateStateBlock failed (0x%08lX)", (unsigned long)hr);
        return;
    }
    D3DVIEWPORT8 oldVp; g_dev->GetViewport(&oldVp);

    SetupRenderState();
    const float W = dd->DisplaySize.x, H = dd->DisplaySize.y;

    for (int n = 0; n < dd->CmdListsCount; n++) {
        const ImDrawList* cl = dd->CmdLists[n];
        g_vtx.resize(cl->VtxBuffer.Size);
        for (int i = 0; i < cl->VtxBuffer.Size; i++) {
            const ImDrawVert& s = cl->VtxBuffer.Data[i];
            g_vtx[i] = { s.pos.x, s.pos.y, 0.0f, (D3DCOLOR)s.col, s.uv.x, s.uv.y };
        }
        for (int c = 0; c < cl->CmdBuffer.Size; c++) {
            const ImDrawCmd* cmd = &cl->CmdBuffer[c];
            if (cmd->UserCallback) {
                if (cmd->UserCallback == ImDrawCallback_ResetRenderState) SetupRenderState();
                else cmd->UserCallback(cl, cmd);
                continue;
            }
            float x0 = cmd->ClipRect.x - dd->DisplayPos.x, y0 = cmd->ClipRect.y - dd->DisplayPos.y;
            float x1 = cmd->ClipRect.z - dd->DisplayPos.x, y1 = cmd->ClipRect.w - dd->DisplayPos.y;
            if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
            if (x1 > W) x1 = W; if (y1 > H) y1 = H;
            if (x1 - x0 < 1.0f || y1 - y0 < 1.0f) continue;
            SetClip(x0, y0, x1, y1);
            g_dev->SetTexture(0, (IDirect3DBaseTexture8*)(intptr_t)cmd->GetTexID());
            UINT numVerts = (UINT)(cl->VtxBuffer.Size - cmd->VtxOffset);
            g_dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, numVerts, cmd->ElemCount / 3,
                                          cl->IdxBuffer.Data + cmd->IdxOffset, D3DFMT_INDEX16,
                                          g_vtx.data() + cmd->VtxOffset, sizeof(Dx8Vertex));
        }
    }

    g_dev->SetViewport(&oldVp);
    g_dev->ApplyStateBlock(saved);
    g_dev->DeleteStateBlock(saved);
}

// Draw-list callback: textures repeat (wrap) instead of clamping, for game
// model textures that tile (the vehicle preview). The renderer's own state
// comes back with ImDrawCallback_ResetRenderState.
void ImGui_ImplDX8_WrapTexturesCallback(const ImDrawList*, const ImDrawCmd*) {
    if (!g_dev) return;
    g_dev->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
    g_dev->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
    g_dev->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
}

// A textured 3D mesh drawn with the depth buffer, inside a screen rectangle
// (the vehicle preview): called from a draw-list callback, so it lands in
// order with the rest of the menu. Vertices are already in screen pixels
// with z = depth 0..1. Solid batches first (depth test + write, cut-out
// texels skipped by alpha test), then see-through ones (test, no write).
// The menu's ImDrawCallback_ResetRenderState callback puts the menu's own
// state back, and the game's state comes back with the state block after.
void ImGui_ImplDX8_DrawDepthMesh(float x0, float y0, float x1, float y1,
                                 const ImGuiDX8MeshVertex* verts, const ImGuiDX8MeshBatch* batches, int numBatches) {
    if (!g_dev || numBatches <= 0 || x1 - x0 < 2 || y1 - y0 < 2) return;
    D3DVIEWPORT8 vp;
    vp.X = (DWORD)(x0 < 0 ? 0 : x0); vp.Y = (DWORD)(y0 < 0 ? 0 : y0);
    vp.Width = (DWORD)(x1 - vp.X); vp.Height = (DWORD)(y1 - vp.Y);
    vp.MinZ = 0.0f; vp.MaxZ = 1.0f;
    g_dev->SetViewport(&vp);
    float L = (float)vp.X + 0.5f, R = (float)(vp.X + vp.Width) + 0.5f;
    float T = (float)vp.Y + 0.5f, B = (float)(vp.Y + vp.Height) + 0.5f;
    D3DMATRIX proj = { { {
        2.0f / (R - L), 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / (T - B), 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        (L + R) / (L - R), (T + B) / (B - T), 0.0f, 1.0f } } };
    g_dev->SetTransform(D3DTS_PROJECTION, &proj);

    IDirect3DSurface8* ds = nullptr;
    bool depth = g_dev->GetDepthStencilSurface(&ds) == D3D_OK && ds;
    if (ds) ds->Release();
    if (depth) {
        D3DRECT r = { (LONG)vp.X, (LONG)vp.Y, (LONG)(vp.X + vp.Width), (LONG)(vp.Y + vp.Height) };
        g_dev->Clear(1, &r, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
    }
    g_dev->SetVertexShader(kFVF);
    g_dev->SetRenderState(D3DRS_ZENABLE, depth ? D3DZB_TRUE : D3DZB_FALSE);
    g_dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    g_dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_dev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    g_dev->SetRenderState(D3DRS_ALPHAREF, 0x20);
    g_dev->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
    g_dev->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
    g_dev->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
    for (int pass = 0; pass < 2; pass++) {
        g_dev->SetRenderState(D3DRS_ZWRITEENABLE, pass == 0 ? TRUE : FALSE);
        g_dev->SetRenderState(D3DRS_ALPHATESTENABLE, pass == 0 ? TRUE : FALSE);
        for (int i = 0; i < numBatches; i++) {
            const ImGuiDX8MeshBatch& b = batches[i];
            if ((b.seeThrough ? 1 : 0) != pass || b.count < 3) continue;
            if (b.texture) {
                g_dev->SetTexture(0, (IDirect3DBaseTexture8*)b.texture);
                g_dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                g_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
            } else {                                                  // plain colour
                g_dev->SetTexture(0, nullptr);
                g_dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
                g_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
            }
            g_dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, b.count / 3, verts + b.first, sizeof(ImGuiDX8MeshVertex));
        }
    }
    g_dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    g_dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
}

// Renders a mesh (see DrawDepthMesh; coordinates 0..size) into a new
// size x size picture with a transparent background and returns it as a
// managed texture (IDirect3DTexture8*), or null. The off-screen target and
// its depth buffer only live for this call, so nothing in video memory is
// left that would block a device reset (alt-tab); managed textures are
// restored by Direct3D itself. Called from a draw-list callback.
void* ImGui_ImplDX8_RenderMeshToTexture(int size, const ImGuiDX8MeshVertex* verts,
                                        const ImGuiDX8MeshBatch* batches, int numBatches, bool* retryLater) {
    *retryLater = false;
    if (!g_dev || size < 8 || size > 1024 || numBatches <= 0) return nullptr;
    if (g_dev->TestCooperativeLevel() != D3D_OK) { *retryLater = true; return nullptr; }   // mid-reset: later
    IDirect3DSurface8 *oldRt = nullptr, *oldDs = nullptr, *rt = nullptr, *ds = nullptr, *sys = nullptr;
    IDirect3DTexture8* out = nullptr;
    bool ok = false;
    g_dev->GetRenderTarget(&oldRt);
    g_dev->GetDepthStencilSurface(&oldDs);
    HRESULT hr;
    if ((hr = g_dev->CreateRenderTarget(size, size, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, FALSE, &rt)) != D3D_OK ||
        (hr = g_dev->CreateDepthStencilSurface(size, size, D3DFMT_D16, D3DMULTISAMPLE_NONE, &ds)) != D3D_OK) {
        // Video memory short or the device lost while drawing: not this model's
        // fault, so the picture is tried again later.
        *retryLater = true;
        static int logs = 0;
        if (logs++ < 5) Log("Spawner picture: off-screen target not made (0x%08lX) - tried again later", (unsigned long)hr);
    } else if (g_dev->SetRenderTarget(rt, ds) == D3D_OK) {
        g_dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0);
        ImGui_ImplDX8_DrawDepthMesh(0, 0, (float)size, (float)size, verts, batches, numBatches);
        // video memory -> system memory -> a managed texture
        if (g_dev->CreateImageSurface(size, size, D3DFMT_A8R8G8B8, &sys) == D3D_OK &&
            g_dev->CopyRects(rt, nullptr, 0, sys, nullptr) == D3D_OK &&
            g_dev->CreateTexture(size, size, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &out) == D3D_OK) {
            D3DLOCKED_RECT src, dst;
            if (sys->LockRect(&src, nullptr, D3DLOCK_READONLY) == D3D_OK) {
                if (out->LockRect(0, &dst, nullptr, 0) == D3D_OK) {
                    for (int y = 0; y < size; y++)
                        memcpy((char*)dst.pBits + y * dst.Pitch, (const char*)src.pBits + y * src.Pitch, size * 4);
                    out->UnlockRect(0);
                    ok = true;
                }
                sys->UnlockRect();
            }
        }
    }
    g_dev->SetRenderTarget(oldRt, oldDs);
    if (oldRt) oldRt->Release();
    if (oldDs) oldDs->Release();
    if (sys) sys->Release();
    if (ds) ds->Release();
    if (rt) rt->Release();
    if (!ok && out) { out->Release(); out = nullptr; }
    if (!ok && g_dev->TestCooperativeLevel() != D3D_OK) *retryLater = true;   // lost while copying
    return out;
}
