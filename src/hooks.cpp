// Game/D3D/window hooks and the ImGui frame driver.
//
//  * main loop -> CGame::Process   : runs queued menu actions + per-frame features
//  * CGame::Process -> UpdatePads   : while the menu is open, feeds the game's mouse
//                                     deltas to ImGui and blanks the pad so the game
//                                     ignores input
//  * IDirect3DDevice8::Present      : draws the menu on top of the finished frame
//  * window procedure               : menu toggle key + keyboard input for ImGui
#include "trainer.h"
#include "game.h"
#include "imgui.h"
#include "imgui_impl_dx8.h"
#include "features.h"
#include <d3d8.h>
#include <float.h>

using namespace game;

bool g_menuOpen = false;
float g_mouseSensitivity = 1.0f;

static WNDPROC g_origWndProc = nullptr;
static bool g_imguiReady = false;
static IDirect3DDevice8* g_device = nullptr;

typedef HRESULT (__stdcall* Present_t)(IDirect3DDevice8*, const RECT*, const RECT*, HWND, const RGNDATA*);
static Present_t g_origPresent = nullptr;

// mouse state captured from the game's DirectInput mouse while the menu is open
static float g_mouseX = 400, g_mouseY = 300;
static bool  g_mouseBtn[3];
static float g_wheel;

// ----------------------------------------------------------------- code patching
static bool PatchCall(DWORD site, DWORD expectedTarget, void* newTarget) {
    unsigned char* p = (unsigned char*)site;
    if (p[0] != 0xE8 || site + 5 + *(int*)(p + 1) != expectedTarget) return false;
    DWORD old;
    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    *(int*)(p + 1) = (int)((DWORD)newTarget - (site + 5));
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    return true;
}

// ----------------------------------------------------------------- keyboard
static ImGuiKey VkToImGuiKey(WPARAM vk) {
    switch (vk) {
    case VK_TAB: return ImGuiKey_Tab;           case VK_LEFT: return ImGuiKey_LeftArrow;
    case VK_RIGHT: return ImGuiKey_RightArrow;  case VK_UP: return ImGuiKey_UpArrow;
    case VK_DOWN: return ImGuiKey_DownArrow;    case VK_PRIOR: return ImGuiKey_PageUp;
    case VK_NEXT: return ImGuiKey_PageDown;     case VK_HOME: return ImGuiKey_Home;
    case VK_END: return ImGuiKey_End;           case VK_DELETE: return ImGuiKey_Delete;
    case VK_BACK: return ImGuiKey_Backspace;    case VK_SPACE: return ImGuiKey_Space;
    case VK_RETURN: return ImGuiKey_Enter;      case VK_ESCAPE: return ImGuiKey_Escape;
    case VK_OEM_MINUS: return ImGuiKey_Minus;   case VK_OEM_PERIOD: return ImGuiKey_Period;
    case VK_CONTROL: return ImGuiKey_LeftCtrl;  case VK_SHIFT: return ImGuiKey_LeftShift;
    case VK_MENU: return ImGuiKey_LeftAlt;
    }
    if (vk >= '0' && vk <= '9') return (ImGuiKey)(ImGuiKey_0 + (vk - '0'));
    if (vk >= 'A' && vk <= 'Z') return (ImGuiKey)(ImGuiKey_A + (vk - 'A'));
    return ImGuiKey_None;
}

static LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // Rebinding a hotkey. A normal key is bound together with whatever of
    // Ctrl/Shift/Alt is held (e.g. Alt + G). A modifier pressed and released on
    // its own is bound alone. Escape clears the binding.
    if (g_capturingHotkey >= 0) {
        static int pendingModifier = 0;
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
            int vk = (int)wp;
            if (vk == VK_ESCAPE) { BindCapturedKey(0, 0); pendingModifier = 0; return 0; }
            if (IsModifierKey(vk)) { pendingModifier = vk; return 0; }
            int mods = CurrentModifiers();
            if (!((vk == VK_INSERT || vk == VK_F7) && mods == 0)) {   // bare INSERT/F7 open the menu
                BindCapturedKey(vk, mods);
                pendingModifier = 0;
                return 0;
            }
        }
        if ((msg == WM_KEYUP || msg == WM_SYSKEYUP) && IsModifierKey((int)wp)) {
            if (pendingModifier == (int)wp) BindCapturedKey((int)wp, 0);
            pendingModifier = 0;
            return 0;
        }
        if (msg == WM_SYSCHAR || msg == WM_CHAR) return 0;
    }
    if (msg == WM_KEYDOWN && (wp == VK_INSERT || wp == VK_F7) && !(lp & (1 << 30))) {
        g_menuOpen = !g_menuOpen;
        g_capturingHotkey = -1;
        if (!g_menuOpen) SaveSettings();
        return 0;
    }
    if (g_imguiReady && g_menuOpen) {
        ImGuiIO& io = ImGui::GetIO();
        switch (msg) {
        case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP: {
            bool down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
            io.AddKeyEvent(ImGuiMod_Ctrl,  (GetKeyState(VK_CONTROL) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiMod_Alt,   (GetKeyState(VK_MENU) & 0x8000) != 0);
            ImGuiKey k = VkToImGuiKey(wp);
            if (k != ImGuiKey_None) io.AddKeyEvent(k, down);
            break;
        }
        case WM_CHAR:
            if (wp > 0 && wp < 0x10000) io.AddInputCharacter((unsigned)wp);
            break;
        }
    }
    return CallWindowProcA(g_origWndProc, hwnd, msg, wp, lp);
}

// ----------------------------------------------------------------- rendering
static void InitImGui(IDirect3DDevice8* dev) {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "VCTrainer_layout.ini";
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.MouseDrawCursor = true; // the game hides the OS cursor
    ApplyTrainerStyle();
    ImGui_ImplDX8_Init(dev);
    g_imguiReady = true;
}

static HRESULT __stdcall HookedPresent(IDirect3DDevice8* dev, const RECT* src, const RECT* dst,
                                       HWND wnd, const RGNDATA* dirty) {
    if (g_imguiReady && (g_menuOpen || OverlayEnabled()) && dev->TestCooperativeLevel() == D3D_OK) {
        static LARGE_INTEGER freq, last;
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        if (!freq.QuadPart) { QueryPerformanceFrequency(&freq); last = now; }
        ImGuiIO& io = ImGui::GetIO();
        float dt = (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart;
        io.DeltaTime = dt > 0.0f ? dt : 1.0f / 60.0f;
        last = now;

        IDirect3DSurface8* bb = nullptr;
        if (dev->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &bb) == D3D_OK) {
            D3DSURFACE_DESC d; bb->GetDesc(&d); bb->Release();
            io.DisplaySize = ImVec2((float)d.Width, (float)d.Height);
        }
        if (g_mouseX > io.DisplaySize.x) g_mouseX = io.DisplaySize.x;
        if (g_mouseY > io.DisplaySize.y) g_mouseY = io.DisplaySize.y;
        io.MouseDrawCursor = g_menuOpen;
        if (g_menuOpen) {
            io.AddMousePosEvent(g_mouseX, g_mouseY);
            for (int i = 0; i < 3; i++) io.AddMouseButtonEvent(i, g_mouseBtn[i]);
            if (g_wheel != 0.0f) { io.AddMouseWheelEvent(0.0f, g_wheel); g_wheel = 0.0f; }
        } else {
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            for (int i = 0; i < 3; i++) { io.AddMouseButtonEvent(i, false); g_mouseBtn[i] = false; }
        }

        ImGui_ImplDX8_NewFrame();
        ImGui::NewFrame();
        DrawTrainerMenu();
        ImGui::Render();
        if (dev->BeginScene() == D3D_OK) {
            ImGui_ImplDX8_RenderDrawData(ImGui::GetDrawData());
            dev->EndScene();
        }
    }
    return g_origPresent(dev, src, dst, wnd, dirty);
}

static void HookDevice(IDirect3DDevice8* dev) {
    void** vtbl = *(void***)dev;
    const int kPresent = 15; // IDirect3DDevice8::Present
    DWORD old;
    VirtualProtect(&vtbl[kPresent], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
    g_origPresent = (Present_t)vtbl[kPresent];
    vtbl[kPresent] = (void*)&HookedPresent;
    VirtualProtect(&vtbl[kPresent], sizeof(void*), old, &old);
    InitImGui(dev);
}

// ----------------------------------------------------------------- game hooks
static void __cdecl HookedUpdatePads() {
    ((void(__cdecl*)())UPDATE_PADS)();
    if (!g_menuOpen) {
        // Fly mode uses WASD/Space/Ctrl/Shift itself: stop the game from also
        // walking, jumping or firing on them. Mouse look is left alone.
        if (Active().flyMode) memset(Pad0, 0, 0x2A);
        return;
    }

    // Undo the game's invert-mouse settings to get raw DirectInput deltas.
    float dx = *(float*)(MouseState + 8)  * (*InvertMouseX ? -1.0f : 1.0f);
    float dy = *(float*)(MouseState + 12) * (*InvertMouseY ? -1.0f : 1.0f);
    g_mouseX += dx * g_mouseSensitivity;
    g_mouseY += dy * g_mouseSensitivity;
    if (g_mouseX < 0) g_mouseX = 0;
    if (g_mouseY < 0) g_mouseY = 0;
    for (int i = 0; i < 3; i++) g_mouseBtn[i] = MouseState[i] != 0;
    if (MouseState[3]) g_wheel += 1.0f;
    if (MouseState[4]) g_wheel -= 1.0f;

    // Blank this frame's controls so the player, camera and weapons ignore input.
    memset(Pad0, 0, 0x2A);            // Pads[0].NewState (CControllerState)
    memset(MouseState, 0, 0x10);      // raw PC mouse state
    memset(NewMouseState, 0, 0x10);   // the copy the camera reads (look stays still)
}

// ----------------------------------------------------------------- bike hook
typedef void* (__thiscall* KnockOff_t)(void* bike, int weapon, int dir, void* ped, bool getBackOn);
static KnockOff_t g_origKnockOff = nullptr;

// __fastcall with an unused edx matches the game's __thiscall (bike in ecx,
// callee pops the 4 stack arguments).
static void* __fastcall HookedKnockOffRider(void* bike, void*, int weapon, int dir, void* ped, bool getBackOn) {
    if (Active().stayOnBike && ped && ped == FindPlayerPed())
        return ped;   // what the original returns; the rider just stays seated
    return g_origKnockOff(bike, weapon, dir, ped, getBackOn);
}

static bool HookKnockOffRider() {
    unsigned char* fn = (unsigned char*)KNOCK_OFF_RIDER;
    // expected prologue: 53 56 57 55 81 EC D0 00 00 00
    const unsigned char expect[KNOCK_OFF_PROLOGUE] = { 0x53, 0x56, 0x57, 0x55, 0x81, 0xEC, 0xD0, 0x00, 0x00, 0x00 };
    if (memcmp(fn, expect, KNOCK_OFF_PROLOGUE) != 0) return false;

    // trampoline = original prologue + jump back into the function
    unsigned char* tramp = (unsigned char*)VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return false;
    memcpy(tramp, fn, KNOCK_OFF_PROLOGUE);
    tramp[KNOCK_OFF_PROLOGUE] = 0xE9;
    *(int*)(tramp + KNOCK_OFF_PROLOGUE + 1) = (int)((DWORD)fn + KNOCK_OFF_PROLOGUE - ((DWORD)tramp + KNOCK_OFF_PROLOGUE + 5));
    g_origKnockOff = (KnockOff_t)tramp;

    DWORD old;
    VirtualProtect(fn, KNOCK_OFF_PROLOGUE, PAGE_EXECUTE_READWRITE, &old);
    fn[0] = 0xE9;
    *(int*)(fn + 1) = (int)((DWORD)&HookedKnockOffRider - ((DWORD)fn + 5));
    memset(fn + 5, 0x90, KNOCK_OFF_PROLOGUE - 5);
    VirtualProtect(fn, KNOCK_OFF_PROLOGUE, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, KNOCK_OFF_PROLOGUE);
    return true;
}

static void __cdecl HookedGameProcess() {
    ((void(__cdecl*)())GAME_PROCESS)();

    // Menu just opened: swing the camera straight behind the player. The mouse
    // is blanked while the menu is open, so it then stays put.
    static bool wasOpen = false;
    if (g_menuOpen && !wasOpen && FindPlayerPed()) CameraBehindPlayer(TheCamera);
    wasOpen = g_menuOpen;

    if (!g_device && *D3DDevice) {
        g_device = (IDirect3DDevice8*)*D3DDevice;
        HookDevice(g_device);
    }
    if (!g_origWndProc && *GameWindow) {
        g_origWndProc = (WNDPROC)SetWindowLongA(*GameWindow, GWL_WNDPROC, (LONG)&HookedWndProc);
        ShowNative("VC Trainer loaded. Press INSERT or F7 to open the menu.");
    }
    TrainerTick();
}

bool InstallHooks() {
    LoadSettings();
    if (!PatchCall(HOOK_PROCESS_SITE, GAME_PROCESS, (void*)&HookedGameProcess)) return false;
    PatchCall(HOOK_PADS_SITE, UPDATE_PADS, (void*)&HookedUpdatePads);
    HookKnockOffRider();
    return true;
}
