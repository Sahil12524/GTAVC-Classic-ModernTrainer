// Minimal Dear ImGui renderer for Direct3D 8.
//
// D3D8 has no scissor test, so each draw command is clipped by setting the
// viewport to its clip rectangle and building an orthographic projection that
// covers exactly that rectangle. Vertex colors already match D3DCOLOR because
// the project is compiled with IMGUI_USE_BGRA_PACKED_COLOR.
#include "imgui.h"
#include "imgui_impl_dx8.h"
#include <d3d8.h>
#include <vector>

struct Dx8Vertex { float x, y, z; D3DCOLOR col; float u, v; };
static const DWORD kFVF = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;

static IDirect3DDevice8*  g_dev = nullptr;
static IDirect3DTexture8* g_fontTex = nullptr;
static std::vector<Dx8Vertex> g_vtx;

static bool CreateFontTexture() {
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* px; int w, h;
    io.Fonts->GetTexDataAsRGBA32(&px, &w, &h);
    if (g_dev->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_fontTex) != D3D_OK)
        return false;
    D3DLOCKED_RECT lr;
    if (g_fontTex->LockRect(0, &lr, nullptr, 0) != D3D_OK) return false;
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
    return true;
}

void ImGui_ImplDX8_Shutdown() {
    if (g_fontTex) { g_fontTex->Release(); g_fontTex = nullptr; }
    g_dev = nullptr;
}

void ImGui_ImplDX8_NewFrame() {
    if (!g_fontTex) CreateFontTexture();
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
    if (g_dev->CreateStateBlock(D3DSBT_ALL, &saved) != D3D_OK) return;
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
