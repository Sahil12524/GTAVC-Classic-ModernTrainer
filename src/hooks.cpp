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
#include <math.h>
#include <stdio.h>
#include <string.h>

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
// Mod compatibility: every hook chains. A call site another mod (CLEO,
// SilentPatch, ...) has already redirected keeps working: we call whatever the
// site pointed to before us, which ends in the game's own function.
static void WriteRel32(unsigned char* p, unsigned char op, DWORD target) {
    DWORD old;
    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    p[0] = op;
    *(int*)(p + 1) = (int)(target - ((DWORD)p + 5));
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
}

// Redirects the E8 call at site to newTarget; returns the previous target (the
// game's function or another mod's hook), or 0 if the site isn't a call.
static DWORD ChainCall(DWORD site, void* newTarget) {
    unsigned char* p = (unsigned char*)site;
    if (p[0] != 0xE8) return 0;
    DWORD prev = site + 5 + *(int*)(p + 1);
    WriteRel32(p, 0xE8, (DWORD)newTarget);
    return prev;
}

// ---- what became of each hook: for VCTrainer.log and the start-up message
enum HookState { HOOK_INSTALLED, HOOK_CHAINED, HOOK_CHANGED, HOOK_NO_MEMORY, HOOK_NOT_A_CALL };
struct HookRecord {
    const char* name; const char* feature; DWORD at;
    HookState state; unsigned char found[10]; int len; DWORD next;
    DWORD own;   // a call site: the game's own function it calls in v1.0 (0 = the main-loop / pads sites)
};
static const int kMaxHooks = 32;
static HookRecord g_hookLog[kMaxHooks];
static int g_hookCount = 0;

static void HexBytes(const unsigned char* b, int n, char* out) {
    out[0] = 0;
    for (int i = 0; i < n; i++) sprintf(out + i * 3, i ? " %02X" : "%02X", b[i]);
}

static HookRecord& NewRecord(const char* name, const char* feature, DWORD at, int len) {
    HookRecord& r = g_hookLog[g_hookCount < kMaxHooks ? g_hookCount++ : kMaxHooks - 1];
    r = HookRecord{ name, feature, at, HOOK_INSTALLED, {}, len, 0, 0 };
    memcpy(r.found, (const void*)at, len);
    return r;
}

static void LogHook(const HookRecord& r, const unsigned char* expect) {
    char found[40], want[40];
    HexBytes(r.found, r.len, found);
    switch (r.state) {
    case HOOK_INSTALLED:
        if (expect) Log("Hook %s (0x%06X): installed (bytes %s, as v1.0)", r.name, r.at, found);
        else if (r.own) Log("Hook %s (0x%06X): installed (bytes %s), calls on to 0x%06X%s", r.name, r.at, found, r.next,
                            r.next == r.own ? " (the game's own)" : " (another mod's hook)");
        else Log("Hook %s (0x%06X): installed, calls on to 0x%06X%s", r.name, r.at, r.next,
                 r.next == (r.at == HOOK_PROCESS_SITE ? GAME_PROCESS : UPDATE_PADS) ? " (the game's own)" : " (another mod's hook)");
        break;
    case HOOK_CHAINED:
        Log("Hook %s (0x%06X): installed through another mod's jump there (to 0x%06X), which is called on", r.name, r.at, r.next);
        break;
    case HOOK_CHANGED:
        HexBytes(expect, r.len, want);
        Log("Hook %s (0x%06X): NOT installed - found %s, v1.0 has %s (another mod changed it). Not working: %s",
            r.name, r.at, found, want, r.feature);
        break;
    case HOOK_NO_MEMORY:
        Log("Hook %s (0x%06X): NOT installed - no memory for its trampoline. Not working: %s", r.name, r.at, r.feature);
        break;
    case HOOK_NOT_A_CALL:
        Log("Hook %s (0x%06X): NOT installed - found %s, not a call (another mod replaced it). Not working: %s",
            r.name, r.at, found, r.feature);
        break;
    }
}

// Puts a jump to `hook` over the first `len` bytes of a game function and
// returns the way to call the original through *orig: a trampoline with those
// bytes and a jump back. Another mod's jump already there (E9) is chained to
// instead. Anything else changed in those bytes - another mod's inline patch -
// and the hook is left out, so nothing half-patched is ever run.
static bool PrologueHook(const char* name, const char* feature, DWORD at, const unsigned char* expect, int len,
                         void* hook, void** orig) {
    unsigned char* fn = (unsigned char*)at;
    HookRecord& r = NewRecord(name, feature, at, len);
    if (fn[0] == 0xE9) {                                   // another mod's jump: chain to it
        r.next = at + 5 + *(int*)(fn + 1);
        *orig = (void*)r.next;
        WriteRel32(fn, 0xE9, (DWORD)hook);
        r.state = HOOK_CHAINED;
        LogHook(r, expect);
        return true;
    }
    if (memcmp(fn, expect, len) != 0) { r.state = HOOK_CHANGED; LogHook(r, expect); return false; }
    unsigned char* tramp = (unsigned char*)VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) { r.state = HOOK_NO_MEMORY; LogHook(r, expect); return false; }
    memcpy(tramp, fn, len);                                // the checked bytes: whole instructions, none relative
    tramp[len] = 0xE9;
    *(int*)(tramp + len + 1) = (int)((DWORD)fn + len - ((DWORD)tramp + len + 5));
    *orig = tramp;
    DWORD old;
    VirtualProtect(fn, len, PAGE_EXECUTE_READWRITE, &old);
    fn[0] = 0xE9;
    *(int*)(fn + 1) = (int)((DWORD)hook - ((DWORD)fn + 5));
    memset(fn + 5, 0x90, len - 5);
    VirtualProtect(fn, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, len);
    LogHook(r, expect);
    return true;
}

// gta-vc.exe v1.0 (3,088,896 bytes): checked by the PE header build stamp, so
// it still matches after other mods have patched code.
static bool IsVC10() {
    char* base = (char*)GetModuleHandleA(nullptr);
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    return nt->FileHeader.TimeDateStamp == 0x504D6947 && base == (char*)0x400000;
}

typedef void (__cdecl* VoidFn)();
static VoidFn g_nextGameProcess = (VoidFn)GAME_PROCESS;
static VoidFn g_nextUpdatePads  = (VoidFn)UPDATE_PADS;

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
    case VK_NUMPAD0: return ImGuiKey_Keypad0;   case VK_NUMPAD2: return ImGuiKey_Keypad2;
    case VK_NUMPAD4: return ImGuiKey_Keypad4;   case VK_NUMPAD5: return ImGuiKey_Keypad5;
    case VK_NUMPAD6: return ImGuiKey_Keypad6;   case VK_NUMPAD8: return ImGuiKey_Keypad8;
    case VK_NUMPAD9: return ImGuiKey_Keypad9;   case VK_NUMPAD3: return ImGuiKey_Keypad3;
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
    // Every key press (not auto-repeat) is remembered with the modifiers held
    // at that moment; the next tick runs its hotkey even if the key is already
    // up again (a tap shorter than a tick).
    if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && !(lp & (1 << 30)))
        NoteKeyPress((int)wp, ((GetKeyState(VK_CONTROL) & 0x8000) ? HKMOD_CTRL : 0) |
                              ((GetKeyState(VK_SHIFT) & 0x8000) ? HKMOD_SHIFT : 0) |
                              ((GetKeyState(VK_MENU) & 0x8000) ? HKMOD_ALT : 0));
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
    static char layoutIni[MAX_PATH];
    GamePath(layoutIni, "VCTrainer_layout.ini");     // the game folder, whatever the working directory
    io.IniFilename = layoutIni;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.MouseDrawCursor = true; // the game hides the OS cursor
    ApplyTrainerStyle();
    LoadUiFonts();
    ImGui_ImplDX8_Init(dev);
    g_imguiReady = true;
}

// The trainer's own cost per drawn frame (everything below up to the game's
// Present), averaged and written to the log once a minute while the enemy
// markers are on, with what the markers' scan cost on the tick.
static void FrameTiming(LONGLONG spent) {
    static LARGE_INTEGER freq;
    static LONGLONG total = 0, windowStart = 0, maxSpent = 0;
    static int frames = 0;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    bool esp = S.enabled && S.enemyEsp;
    if (!esp) { frames = 0; total = maxSpent = 0; windowStart = now.QuadPart; return; }
    if (!windowStart) windowStart = now.QuadPart;
    total += spent; frames++;
    if (spent > maxSpent) maxSpent = spent;
    double secs = (double)(now.QuadPart - windowStart) / (double)freq.QuadPart;
    if (secs < 60.0) return;
    double scanMs = 0; int scans = 0, marks = 0;
    TakeEspScanStats(&scanMs, &scans, &marks);
    double avgMs = frames ? (double)total * 1000.0 / (double)freq.QuadPart / frames : 0.0;
    Log("Timing: trainer's part of Present %.3f ms per frame (max %.3f) over %d frames, %.0f FPS, menu %s, overlay %s; "
        "ESP scan on the tick %.3f ms x %d (%.1f enemies marked)",
        avgMs, (double)maxSpent * 1000.0 / (double)freq.QuadPart, frames, frames / secs,
        g_menuOpen ? "open" : "closed", OverlayEnabled() ? "on" : "off",
        scans ? scanMs / scans : 0.0, scans, scans ? (double)marks / scans : 0.0);
    frames = 0; total = maxSpent = 0; windowStart = now.QuadPart;
}

static HRESULT __stdcall HookedPresent(IDirect3DDevice8* dev, const RECT* src, const RECT* dst,
                                       HWND wnd, const RGNDATA* dirty) {
    LARGE_INTEGER t0; QueryPerformanceCounter(&t0);
    PollMouseHotkeys();                               // mouse-button hotkeys: every drawn frame
    if (g_imguiReady && (g_menuOpen || AnyOverlayOn()) && dev->TestCooperativeLevel() == D3D_OK) {
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

        UiBeforeNewFrame();
        ImGui_ImplDX8_NewFrame();
        ImGui::NewFrame();
        DrawTrainerMenu();
        ImGui::Render();
        if (dev->BeginScene() == D3D_OK) {
            ImGui_ImplDX8_RenderDrawData(ImGui::GetDrawData());
            dev->EndScene();
        }
    }
    LARGE_INTEGER t1; QueryPerformanceCounter(&t1);
    FrameTiming(t1.QuadPart - t0.QuadPart);
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
    Log("Hook IDirect3DDevice8::Present (vtable slot 15): installed, device 0x%08X, previous Present 0x%08X",
        (DWORD)dev, (DWORD)g_origPresent);
    InitImGui(dev);
}

// ----------------------------------------------------------------- game hooks
static void __cdecl HookedUpdatePads() {
    g_nextUpdatePads();
    // The aim locks' camera turns decided last tick go in now: CGame::Process
    // has just started (this is its first call), so the game builds this
    // tick's camera from them instead of showing them a tick late.
    if (!game::ReplayPlaying()) ApplyPendingCameraTurns();
    if (!game::ReplayPlaying()) DoorLockBeforeInput();   // locked car: let the player out
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
// "Never fall off bike" skips the knock-off only where that is safe: an older
// version skipped every call and left the player half-off (stuck on the bike)
// because some callers - the carjack/drag paths - set the ped as already being
// pulled off before calling. The crash sites below change nothing before the
// call (checked in the v1.0 code), so skipping them just keeps the player on.
//
// Every place the game calls KnockOffRider (v1.0 call sites), by why:
//  crashes / falls / hits - blocked outright, so the player simply stays on:
//    0x60A197 0x60A1AF  CBike collision, RAMMEDBYCAR
//    0x614C6B 0x614CE0  bike hitting / hit by something, RAMMEDBYCAR
//    0x522C17 0x522C2F 0x522D33 0x522D4B  rider's own fall check, FALL
//    0x526960           CPed::InflictDamage (shot, run into)
//    0x5D31E2           melee hit
//  a ped taking the bike - always allowed, never undone:
//    0x5169C1 0x516BEE  CPed being dragged off by a carjacker / cop
//    0x5009E9           ped animation event (jack sequence)
//  bike sinking - 0x6137D3 0x6138A0 (DROWNING) - allowed, never undone.
// Anything else (another mod, an unknown path) falls back to the old way:
// the knock-off runs and the player is put back on next frame.
static bool CalledFrom(const DWORD* sites, int n, DWORD ret) {
    for (int i = 0; i < n; i++) if (ret == sites[i] + 5) return true;
    return false;
}
static const DWORD kCrashSites[] = { 0x60A197, 0x60A1AF, 0x614C6B, 0x614CE0, 0x522C17, 0x522C2F,
                                     0x522D33, 0x522D4B, 0x526960, 0x5D31E2 };
static const DWORD kLetGoSites[] = { 0x5169C1, 0x516BEE, 0x5009E9, 0x6137D3, 0x6138A0 };

static void* __fastcall HookedKnockOffRider(void* bike, void*, int weapon, int dir, void* ped, bool getBackOn) {
    DWORD ret = (DWORD)__builtin_return_address(0);
    bool player = ped && ped == FindPlayerPed();
    if (player && Active().stayOnBike && !BikeExitRequested()) {
        if (CalledFrom(kCrashSites, 10, ret)) return nullptr;    // stay on: no fall, no damage
        if (!CalledFrom(kLetGoSites, 5, ret)) BikeKnockedOff(bike);  // unknown: re-seat after
    }
    return g_origKnockOff(bike, weapon, dir, ped, getBackOn);
}

// Takes the rider off through the game's own routine, without re-seating.
void ForceOffBike(void* bike, void* ped) {
    // weapon type 44 = WEAPONTYPE_FALL (43 is DROWNING: water splash + sound)
    if (g_origKnockOff && bike && ped) g_origKnockOff(bike, 0x2C, 0, ped, false);
}

static bool HookKnockOffRider() {
    // expected prologue: 53 56 57 55 81 EC D0 00 00 00 (push ebx/esi/edi/ebp; sub esp, 0xD0)
    static const unsigned char expect[KNOCK_OFF_PROLOGUE] = { 0x53, 0x56, 0x57, 0x55, 0x81, 0xEC, 0xD0, 0x00, 0x00, 0x00 };
    return PrologueHook("CBike::KnockOffRider", "Never fall off bike", KNOCK_OFF_RIDER, expect, KNOCK_OFF_PROLOGUE,
                        (void*)&HookedKnockOffRider, (void**)&g_origKnockOff);
}

// ---------------------------------------------------------------- wanted hook
// CWanted::UpdateWantedLevel (0x4D2110) turns the "chaos" points into stars
// and is run by everything that changes the wanted level: crimes, scripts,
// cheats, SetWantedLevel. Clearing once per frame left whatever was added
// later in the frame in effect (stars came back, cops still busted). Here the
// player's chaos is zeroed every time the stars are worked out, so with never
// wanted on (or right after Clear) no star can appear at all. The game's
// maximum-wanted globals are not touched: they end up in save games.
typedef void (__thiscall* UpdWanted_t)(void* wanted);
static UpdWanted_t g_origUpdWanted = nullptr;

static void __fastcall HookedUpdateWantedLevel(void* wanted, void*) {
    bool hold = false;
    if (wanted && HoldNoWanted()) {
        void* ped = FindPlayerPed();
        if (ped && at<void*>(ped, PED_WANTED) == wanted) { *(int*)wanted = 0; hold = true; }   // m_nChaos
    }
    // Stars picked above the story's limit: lift it for the player's update
    // only, so the chaos isn't clamped back down (put back right after).
    bool mine = false;
    if (wanted && g_wantedAboveLimit) {
        void* ped = FindPlayerPed();
        mine = ped && at<void*>(ped, PED_WANTED) == wanted;
    }
    if (hold && mine) g_wantedAboveLimit = 0;
    {
        WantedLimitLift lift(mine && g_wantedAboveLimit != 0);
        g_origUpdWanted(wanted);
        // Back down to within the game's own limit (cleared, spray shop,
        // busted, a script): the lift is no longer wanted.
        if (lift.on && at<int>(wanted, WANTED_LEVEL) <= lift.savedLevel) g_wantedAboveLimit = 0;
    }
    // Wanted level lock: the stars just worked out may not go past the
    // player's chosen maximum (SetWantedLevel also resets the chaos to match,
    // and its own call back in here then finds the level within the limit).
    if (wanted && !hold) {
        int cap = WantedCap();
        void* ped = FindPlayerPed();
        if (cap >= 0 && ped && at<void*>(ped, PED_WANTED) == wanted && at<int>(wanted, WANTED_LEVEL) > cap)
            SetWantedLevel(wanted, cap);
    }
    // (Raising m_MaximumLawEnforcerVehicles here for more patrol cars was
    // tried and removed: the game sends those cars to the player and the
    // cops get out, even at zero stars.)
    (void)hold;
}

static bool HookUpdateWantedLevel() {
    // expected: 8B 15 DC 10 69 00 (mov edx, [MaximumChaosLevel]) - position-independent
    static const unsigned char expect[6] = { 0x8B, 0x15, 0xDC, 0x10, 0x69, 0x00 };
    return PrologueHook("CWanted::UpdateWantedLevel", "Never wanted, wanted level lock", 0x4D2110, expect, 6,
                        (void*)&HookedUpdateWantedLevel, (void**)&g_origUpdWanted);
}

// CPed::SetArrestPlayer (0x4EB5F0, cop = this, the suspect as argument): every
// bust goes through it - on foot, or a cop opening the car door and pulling
// the player out. A cop already on its way when the stars were cleared still
// got here, so the arrest itself is refused while never wanted holds.
typedef void (__thiscall* Arrest_t)(void* cop, void* suspect);
static Arrest_t g_origArrest = nullptr;

static void __fastcall HookedSetArrestPlayer(void* cop, void*, void* suspect) {
    if (suspect && suspect == FindPlayerPed() && HoldNoWanted()) return;
    g_origArrest(cop, suspect);
}

static bool HookSetArrestPlayer() {
    // expected: 53 55 89 CD 8B 5C 24 0C (push ebx; push ebp; mov ebp,ecx; mov ebx,[esp+0C])
    static const unsigned char expect[8] = { 0x53, 0x55, 0x89, 0xCD, 0x8B, 0x5C, 0x24, 0x0C };
    return PrologueHook("CPed::SetArrestPlayer", "No busts while never wanted", 0x4EB5F0, expect, 8,
                        (void*)&HookedSetArrestPlayer, (void**)&g_origArrest);
}

// CVehicle::KillPedsInVehicle (0x5B8560): run by every vehicle type's
// BlowUpCar; kills whoever is inside with SetDead (no health/proofs check).
// With "survive vehicle explosions" on, the player is taken out first.
typedef void (__thiscall* KillPeds_t)(void* veh);
static KillPeds_t g_origKillPeds = nullptr;

static void __fastcall HookedKillPedsInVehicle(void* veh, void*) {
    void** seat = nullptr;
    void* who = nullptr;
    if (veh && SurviveBlowUpOn()) {
        if (BlowUpStayInside()) {
            // The kill only looks at the seats: leave the player's seat empty
            // just for that, then sit them straight back in the wreck.
            if ((seat = PlayerSeatInBlast(veh))) { who = *seat; *seat = nullptr; }
        } else {
            EjectPlayerFromBlast(veh);
        }
    }
    g_origKillPeds(veh);
    if (seat) *seat = who;
}

static bool HookKillPedsInVehicle() {
    // expected: 53 89 CB 8B 83 A8 01 00 00 (push ebx; mov ebx,ecx; mov eax,[ebx+1A8])
    static const unsigned char expect[9] = { 0x53, 0x89, 0xCB, 0x8B, 0x83, 0xA8, 0x01, 0x00, 0x00 };
    return PrologueHook("CVehicle::KillPedsInVehicle", "Survive vehicle explosions", 0x5B8560, expect, 9,
                        (void*)&HookedKillPedsInVehicle, (void**)&g_origKillPeds);
}

// CPed::SetFall (0x4FD9F0, thiscall(extraTime, animId, evenIfNotInControl)):
// every knock-down - hit by a car, blasted by an explosion, shoved, tripped -
// starts here. With "no ragdoll" on, the player just isn't knocked over.
typedef void (__thiscall* SetFall_t)(void* ped, int extraTime, int animId, bool evenIfNotInControl);
static SetFall_t g_origSetFall = nullptr;

// The game's own get-in / get-out of car code (0x516000-0x51C000, and the
// jump-out at 0x5860CE: rolling out of a moving car, being pulled out) ends its animation with SetFall and
// gets the ped up from it. Blocked, the player was left lying flat for good
// after a roll-out - so those falls always go through.
static void __fastcall HookedSetFall(void* ped, void*, int extraTime, int animId, bool evenIfNotInControl) {
    DWORD from = (DWORD)__builtin_return_address(0);
    bool carExit = (from >= 0x516000 && from < 0x51C000) ||
                   from == 0x5860D3;   // the vehicle's jump-out: car speed given to the ped, then knocked down (anim 0xAD)
    if (ped && ped == FindPlayerPed() && carExit) NoteCarExitFall();
    if (ped && ped == FindPlayerPed() && NoRagdollOn() && !carExit) return;
    g_origSetFall(ped, extraTime, animId, evenIfNotInControl);
}

static bool HookSetFall() {
    // expected: 53 56 57 55 89 CD (push ebx; push esi; push edi; push ebp; mov ebp,ecx)
    static const unsigned char expect[6] = { 0x53, 0x56, 0x57, 0x55, 0x89, 0xCD };
    return PrologueHook("CPed::SetFall", "No ragdoll, walk on water landings", 0x4FD9F0, expect, 6,
                        (void*)&HookedSetFall, (void**)&g_origSetFall);
}

// CWeapon::Fire (0x5D45E0, thiscall(shooter, source), ret 8): every shot by
// every gun. The trainer counts the player's shots here (one-shot vehicles,
// wall bang, bullet penetration) and stretches the range for penetration.
typedef bool (__thiscall* WeaponFire_t)(void* weapon, void* shooter, float* source);
static WeaponFire_t g_origWeaponFire = nullptr;

static bool __fastcall HookedWeaponFire(void* weapon, void*, void* shooter, float* source) {
    return TrainerWeaponFire(weapon, shooter, source, g_origWeaponFire);
}

static bool HookWeaponFire() {
    // expected: 53 56 57 55 83 EC 28 (push ebx; push esi; push edi; push ebp; sub esp, 0x28)
    static const unsigned char expect[7] = { 0x53, 0x56, 0x57, 0x55, 0x83, 0xEC, 0x28 };
    return PrologueHook("CWeapon::Fire", "Wall bang, one-shot vehicles, bullet penetration, damage multiplier, "
                        "explosive / fire bullets, teleport / delete / vehicle guns, Hunter aimbot's shot count",
                        0x5D45E0, expect, 7, (void*)&HookedWeaponFire, (void**)&g_origWeaponFire);
}

static void ShowStartupMessage();
static void __cdecl HookedGameProcess() {
    g_nextGameProcess();

    // Opening the menu leaves the camera wherever it looks: the mouse is
    // blanked while the menu is open, so the view simply holds still.

    if (!g_device && *D3DDevice) {
        g_device = (IDirect3DDevice8*)*D3DDevice;
        HookDevice(g_device);
    }
    if (!g_origWndProc && *GameWindow) {
        g_origWndProc = (WNDPROC)SetWindowLongA(*GameWindow, GWL_WNDPROC, (LONG)&HookedWndProc);
        if (g_origWndProc) Log("Hook window procedure: installed (previous 0x%08X)", (DWORD)g_origWndProc);
        else Log("Hook window procedure: NOT installed (SetWindowLong error %lu). Not working: menu key, hotkeys", GetLastError());
        ShowStartupMessage();
    }
    TrainerTick();
}

// A redirected call site, recorded like the prologue hooks.
static DWORD ChainCallLogged(const char* name, const char* feature, DWORD site, void* hook) {
    HookRecord& r = NewRecord(name, feature, site, 5);
    r.next = ChainCall(site, hook);
    r.state = r.next ? HOOK_INSTALLED : HOOK_NOT_A_CALL;
    LogHook(r, nullptr);
    return r.next;
}

// A call site whose v1.0 target is known (logged as the game's own or another mod's).
static DWORD ChainCallOwn(const char* name, const char* feature, DWORD site, DWORD own, void* hook) {
    HookRecord& r = NewRecord(name, feature, site, 5);
    r.own = own;
    r.next = ChainCall(site, hook);
    r.state = r.next ? HOOK_INSTALLED : HOOK_NOT_A_CALL;
    LogHook(r, nullptr);
    return r.next;
}

// ---------------------------------------------------------------- drive-by shots
// DoDriveByShootings (bike 0x5C91E0, boat 0x5C9540, car 0x5C97B0) fires with
// CWeapon::FireFromCar (0x5D44E0, thiscall(vehicle, left, right), ret 0Ch) right
// before each "timer = now + 70 ms":
//   0x5C949C  E8 3F B0 00 00, after FF 34 24 FF 74 24 08 55     bike: push right; push left; push ebp
//   0x5C9732  E8 A9 AD 00 00, after 6A 01 FF 74 24 0C 57        boat: push 1; push left; push edi
//   0x5C99DE  E8 FD AA 00 00, after 8A 44 24 08 89 D9 6A 01 50 56  car: push 1; push al (left); push esi
// The hook records the gun, vehicle and side the game chose this tick, so rapid
// fire adds its extra shots the same way (combat.cpp).
static const DWORD FIRE_FROM_CAR = 0x5D44E0;
static DWORD g_fromCarNext[3] = { FIRE_FROM_CAR, FIRE_FROM_CAR, FIRE_FROM_CAR };
typedef bool (__thiscall* FromCar_t)(void* weapon, void* veh, int left, int right);
template<int N> static bool __fastcall HookedFireFromCar(void* weapon, void*, void* veh, int left, int right) {
    bool ok = ((FromCar_t)g_fromCarNext[N])(weapon, veh, left, right);
    if (ok) TrainerDriveByShot(weapon, veh, left & 0xFF, right & 0xFF);
    return ok;
}

// ---------------------------------------------------------------- Hunter shots
// CAutomobile::ProcessControl fires the Hunter's weapons with a temporary CWeapon
// on the stack, straight into the shot functions (never CWeapon::Fire):
//   0x5980B0, 0x5981D0  FireProjectile (0x5CCF90, thiscall(shooter, source, power)):
//                       push [69A5E4]; push eax; push ebp; call - left / right rocket pod
//   0x5982B6, 0x5983F3  FireInstantHit (0x5D1140, thiscall(shooter, source)):
//                       push eax; push ebp; call - the cannon
// Each is called through, then counted (combat.cpp: wall bang, one-shot vehicles,
// penetration and the heli aimbot see the Hunter's shots).
static const DWORD FIRE_PROJECTILE = 0x5CCF90, FIRE_INSTANT_HIT = 0x5D1140;
static DWORD g_heliNext[4] = { FIRE_PROJECTILE, FIRE_PROJECTILE, FIRE_INSTANT_HIT, FIRE_INSTANT_HIT };
typedef bool (__thiscall* Projectile_t)(void* weapon, void* shooter, float* source, float power);
typedef bool (__thiscall* InstantHit_t)(void* weapon, void* shooter, float* source);
template<int N> static bool __fastcall HookedHeliRocket(void* weapon, void*, void* shooter, float* source, float power) {
    bool ok = ((Projectile_t)g_heliNext[N])(weapon, shooter, source, power);
    TrainerHeliShot(weapon, shooter, source, ok);
    return ok;
}
template<int N> static bool __fastcall HookedHeliCannon(void* weapon, void*, void* shooter, float* source) {
    bool ok = ((InstantHit_t)g_heliNext[N])(weapon, shooter, source);
    TrainerHeliShot(weapon, shooter, source, ok);
    return ok;
}

static void HookVehicleShots() {
    static const char* kDriveBy = "Rapid fire in drive-bys";
    static const DWORD kFromCar[3] = { 0x5C949C, 0x5C9732, 0x5C99DE };
    void* const fromCar[3] = { (void*)&HookedFireFromCar<0>, (void*)&HookedFireFromCar<1>, (void*)&HookedFireFromCar<2> };
    static const char* const kFromCarName[3] = { "bike drive-by -> CWeapon::FireFromCar", "boat drive-by -> CWeapon::FireFromCar",
                                                 "car drive-by -> CWeapon::FireFromCar" };
    for (int i = 0; i < 3; i++)
        if (DWORD p = ChainCallOwn(kFromCarName[i], kDriveBy, kFromCar[i], FIRE_FROM_CAR, fromCar[i])) g_fromCarNext[i] = p;

    static const char* kHeli = "Hunter shot count (wall bang, one-shot vehicles, penetration, heli aimbot)";
    static const DWORD kHeliSites[4] = { 0x5980B0, 0x5981D0, 0x5982B6, 0x5983F3 };
    static const DWORD kHeliOwn[4] = { FIRE_PROJECTILE, FIRE_PROJECTILE, FIRE_INSTANT_HIT, FIRE_INSTANT_HIT };
    void* const heli[4] = { (void*)&HookedHeliRocket<0>, (void*)&HookedHeliRocket<1>,
                            (void*)&HookedHeliCannon<2>, (void*)&HookedHeliCannon<3> };
    static const char* const kHeliName[4] = { "Hunter left rocket -> FireProjectile", "Hunter right rocket -> FireProjectile",
                                              "Hunter cannon -> FireInstantHit", "Hunter cannon (2) -> FireInstantHit" };
    for (int i = 0; i < 4; i++)
        if (DWORD p = ChainCallOwn(kHeliName[i], kHeli, kHeliSites[i], kHeliOwn[i], heli[i])) g_heliNext[i] = p;
}

// ---------------------------------------------------------------- wide-FOV crosshair
// CCamera::Find3rdPersonCamTargetVector (0x46F890; called only by the weapon
// fire code) places the shot's ray with the CCam FOV, while the frame is drawn
// with the widened CDraw FOV, so at a wide FOV slider the bullet leaves the
// crosshair. The hook builds the ray with the drawn FOV (combat.cpp
// WideCrosshairTarget) and otherwise calls the game's own. Only on the v1.0
// bytes: a jump from another mod there is not chained, the aiming FOV is then
// just left at the game's.
bool g_wideCrosshairHook = false;
typedef void (__thiscall* Find3rd_t)(void* camera, float dist, float px, float py, float pz, float* source, float* target);
static Find3rd_t g_origFind3rd = nullptr;
static void __fastcall HookedFind3rdPersonCamTargetVector(void* camera, void*, float dist, float px, float py, float pz,
                                                          float* source, float* target) {
    float pos[3] = { px, py, pz };
    if (!WideCrosshairTarget(dist, pos, source, target)) g_origFind3rd(camera, dist, px, py, pz, source, target);
}

static void HookFind3rdPersonCamTargetVector() {
    static const unsigned char expect[8] = { 0x53, 0x56, 0x55, 0x89, 0xCD, 0x83, 0xEC, 0x28 };
    if (memcmp((const void*)FIND_3RD_PERSON_TARGET, expect, sizeof expect) != 0) {
        Log("Hook CCamera::Find3rdPersonCamTargetVector (0x46F890): not v1.0 or hooked by another mod; aiming keeps the game's FOV");
        return;
    }
    if (PrologueHook("CCamera::Find3rdPersonCamTargetVector", "FOV slider while aiming", FIND_3RD_PERSON_TARGET, expect,
                     sizeof expect, (void*)&HookedFind3rdPersonCamTargetVector, (void**)&g_origFind3rd)) {
        g_wideCrosshairHook = true;
        Log("Hook CCamera::Find3rdPersonCamTargetVector (0x46F890): installed, shots follow the drawn crosshair at any FOV");
    }
}

bool InstallHooks() {
    if (!IsVC10()) {
        Log("Modern VC Trainer v%s: this gta-vc.exe is not v1.0 (build stamp / base differ) - nothing hooked", VCTRAINER_VERSION);
        return false;
    }
    Log("---- Modern VC Trainer v%s starting (gta-vc.exe v1.0, build stamp 504D6947)", VCTRAINER_VERSION);
    LoadSettings();
    DWORD prev = ChainCallLogged("main loop -> CGame::Process", "the whole trainer", HOOK_PROCESS_SITE, (void*)&HookedGameProcess);
    if (!prev) {
        MessageBoxA(nullptr, "Modern VC Trainer: another mod has replaced the game's main-loop call, "
                    "so the trainer can't start.", "Modern VC Trainer", MB_ICONWARNING);
        return true;   // the version is fine; don't also show the version warning
    }
    g_nextGameProcess = (VoidFn)prev;
    if (DWORD p = ChainCallLogged("CGame::Process -> CPad::UpdatePads", "Menu mouse cursor, game input blocked while the menu "
                                  "is open, fly mode's key blanking", HOOK_PADS_SITE, (void*)&HookedUpdatePads))
        g_nextUpdatePads = (VoidFn)p;
    HookKnockOffRider();
    HookUpdateWantedLevel();
    HookSetArrestPlayer();
    HookKillPedsInVehicle();
    HookSetFall();
    HookWeaponFire();
    HookVehicleShots();
    HookFind3rdPersonCamTargetVector();
    int ok = 0;
    for (int i = 0; i < g_hookCount; i++) ok += g_hookLog[i].state == HOOK_INSTALLED || g_hookLog[i].state == HOOK_CHAINED;
    Log("Hooks: %d of %d game hooks installed", ok, g_hookCount);
    return true;
}

// The start-up help-box line: loaded, and which features a failed hook switched off.
static void ShowStartupMessage() {
    char off[200] = "";
    int n = 0;
    for (int i = 0; i < g_hookCount; i++) {
        const HookRecord& r = g_hookLog[i];
        if (r.state == HOOK_INSTALLED || r.state == HOOK_CHAINED) continue;
        // the short name: up to the first comma of the feature list
        char f[48]; lstrcpynA(f, r.feature, sizeof f);
        if (char* c = strchr(f, ',')) *c = 0;
        n += _snprintf(off + n, sizeof off - n - 1, "%s%s", n ? ", " : "", f);
        if (n < 0 || n >= (int)sizeof off - 2) { n = (int)strlen(off); break; }
    }
    if (n) ShowNative("Modern VC Trainer v%s loaded (INSERT/F7). Not working - another mod changed the game there: %s. See VCTrainer.log",
                      VCTRAINER_VERSION, off);
    else   ShowNative("Modern VC Trainer v%s loaded. Press INSERT or F7 to open the menu.", VCTRAINER_VERSION);
}
