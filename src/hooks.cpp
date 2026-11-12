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
    g_nextUpdatePads();
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
    unsigned char* fn = (unsigned char*)KNOCK_OFF_RIDER;
    // Another mod already put a jump here: chain to it instead of a trampoline.
    if (fn[0] == 0xE9) {
        g_origKnockOff = (KnockOff_t)((DWORD)fn + 5 + *(int*)(fn + 1));
        WriteRel32(fn, 0xE9, (DWORD)&HookedKnockOffRider);
        return true;
    }
    // expected prologue: 53 56 57 55 81 EC D0 00 00 00
    const unsigned char expect[KNOCK_OFF_PROLOGUE] = { 0x53, 0x56, 0x57, 0x55, 0x81, 0xEC, 0xD0, 0x00, 0x00, 0x00 };
    if (memcmp(fn, expect, KNOCK_OFF_PROLOGUE) != 0) return false;   // changed in an unknown way: skip

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
    g_origUpdWanted(wanted);
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
    unsigned char* fn = (unsigned char*)0x4D2110;
    if (fn[0] == 0xE9) {                     // another mod's jump: chain to it
        g_origUpdWanted = (UpdWanted_t)((DWORD)fn + 5 + *(int*)(fn + 1));
        WriteRel32(fn, 0xE9, (DWORD)&HookedUpdateWantedLevel);
        return true;
    }
    // expected: 8B 15 DC 10 69 00 (mov edx, [MaximumChaosLevel]) - position-independent
    const unsigned char expect[6] = { 0x8B, 0x15, 0xDC, 0x10, 0x69, 0x00 };
    if (memcmp(fn, expect, 6) != 0) return false;
    unsigned char* tramp = (unsigned char*)VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return false;
    memcpy(tramp, fn, 6);
    tramp[6] = 0xE9;
    *(int*)(tramp + 7) = (int)((DWORD)fn + 6 - ((DWORD)tramp + 11));
    g_origUpdWanted = (UpdWanted_t)tramp;
    DWORD old;
    VirtualProtect(fn, 6, PAGE_EXECUTE_READWRITE, &old);
    fn[0] = 0xE9;
    *(int*)(fn + 1) = (int)((DWORD)&HookedUpdateWantedLevel - ((DWORD)fn + 5));
    fn[5] = 0x90;
    VirtualProtect(fn, 6, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 6);
    return true;
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
    unsigned char* fn = (unsigned char*)0x4EB5F0;
    if (fn[0] == 0xE9) {
        g_origArrest = (Arrest_t)((DWORD)fn + 5 + *(int*)(fn + 1));
        WriteRel32(fn, 0xE9, (DWORD)&HookedSetArrestPlayer);
        return true;
    }
    // expected: 53 55 89 CD 8B 5C 24 0C (push ebx; push ebp; mov ebp,ecx; mov ebx,[esp+0C])
    const unsigned char expect[8] = { 0x53, 0x55, 0x89, 0xCD, 0x8B, 0x5C, 0x24, 0x0C };
    if (memcmp(fn, expect, 8) != 0) return false;
    unsigned char* tramp = (unsigned char*)VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return false;
    memcpy(tramp, fn, 8);
    tramp[8] = 0xE9;
    *(int*)(tramp + 9) = (int)((DWORD)fn + 8 - ((DWORD)tramp + 13));
    g_origArrest = (Arrest_t)tramp;
    DWORD old;
    VirtualProtect(fn, 8, PAGE_EXECUTE_READWRITE, &old);
    fn[0] = 0xE9;
    *(int*)(fn + 1) = (int)((DWORD)&HookedSetArrestPlayer - ((DWORD)fn + 5));
    memset(fn + 5, 0x90, 3);
    VirtualProtect(fn, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 8);
    return true;
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
    unsigned char* fn = (unsigned char*)0x5B8560;
    if (fn[0] == 0xE9) {
        g_origKillPeds = (KillPeds_t)((DWORD)fn + 5 + *(int*)(fn + 1));
        WriteRel32(fn, 0xE9, (DWORD)&HookedKillPedsInVehicle);
        return true;
    }
    // expected: 53 89 CB 8B 83 A8 01 00 00 (push ebx; mov ebx,ecx; mov eax,[ebx+1A8])
    const unsigned char expect[9] = { 0x53, 0x89, 0xCB, 0x8B, 0x83, 0xA8, 0x01, 0x00, 0x00 };
    if (memcmp(fn, expect, 9) != 0) return false;
    unsigned char* tramp = (unsigned char*)VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return false;
    memcpy(tramp, fn, 9);
    tramp[9] = 0xE9;
    *(int*)(tramp + 10) = (int)((DWORD)fn + 9 - ((DWORD)tramp + 14));
    g_origKillPeds = (KillPeds_t)tramp;
    DWORD old;
    VirtualProtect(fn, 9, PAGE_EXECUTE_READWRITE, &old);
    fn[0] = 0xE9;
    *(int*)(fn + 1) = (int)((DWORD)&HookedKillPedsInVehicle - ((DWORD)fn + 5));
    memset(fn + 5, 0x90, 4);
    VirtualProtect(fn, 9, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 9);
    return true;
}

// CPed::SetFall (0x4FD9F0, thiscall(extraTime, animId, evenIfNotInControl)):
// every knock-down - hit by a car, blasted by an explosion, shoved, tripped -
// starts here. With "no ragdoll" on, the player just isn't knocked over.
typedef void (__thiscall* SetFall_t)(void* ped, int extraTime, int animId, bool evenIfNotInControl);
static SetFall_t g_origSetFall = nullptr;

static void __fastcall HookedSetFall(void* ped, void*, int extraTime, int animId, bool evenIfNotInControl) {
    if (ped && ped == FindPlayerPed() && NoRagdollOn()) return;
    g_origSetFall(ped, extraTime, animId, evenIfNotInControl);
}

static bool HookSetFall() {
    unsigned char* fn = (unsigned char*)0x4FD9F0;
    if (fn[0] == 0xE9) {
        g_origSetFall = (SetFall_t)((DWORD)fn + 5 + *(int*)(fn + 1));
        WriteRel32(fn, 0xE9, (DWORD)&HookedSetFall);
        return true;
    }
    // expected: 53 56 57 55 89 CD (push ebx; push esi; push edi; push ebp; mov ebp,ecx)
    const unsigned char expect[6] = { 0x53, 0x56, 0x57, 0x55, 0x89, 0xCD };
    if (memcmp(fn, expect, 6) != 0) return false;
    unsigned char* tramp = (unsigned char*)VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return false;
    memcpy(tramp, fn, 6);
    tramp[6] = 0xE9;
    *(int*)(tramp + 7) = (int)((DWORD)fn + 6 - ((DWORD)tramp + 11));
    g_origSetFall = (SetFall_t)tramp;
    DWORD old;
    VirtualProtect(fn, 6, PAGE_EXECUTE_READWRITE, &old);
    fn[0] = 0xE9;
    *(int*)(fn + 1) = (int)((DWORD)&HookedSetFall - ((DWORD)fn + 5));
    fn[5] = 0x90;
    VirtualProtect(fn, 6, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 6);
    return true;
}

static void __cdecl HookedGameProcess() {
    g_nextGameProcess();

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
        ShowNative("Modern VC Trainer loaded. Press INSERT or F7 to open the menu.");
    }
    TrainerTick();
}

bool InstallHooks() {
    if (!IsVC10()) return false;
    LoadSettings();
    DWORD prev = ChainCall(HOOK_PROCESS_SITE, (void*)&HookedGameProcess);
    if (!prev) {
        MessageBoxA(nullptr, "Modern VC Trainer: another mod has replaced the game's main-loop call, "
                    "so the trainer can't start.", "Modern VC Trainer", MB_ICONWARNING);
        return true;   // the version is fine; don't also show the version warning
    }
    g_nextGameProcess = (VoidFn)prev;
    if (DWORD p = ChainCall(HOOK_PADS_SITE, (void*)&HookedUpdatePads)) g_nextUpdatePads = (VoidFn)p;
    HookKnockOffRider();
    HookUpdateWantedLevel();
    HookSetArrestPlayer();
    HookKillPedsInVehicle();
    HookSetFall();
    return true;
}
