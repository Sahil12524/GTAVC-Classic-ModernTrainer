// One DLL, two ways to load it:
//  * VCTrainer.asi - loaded by Ultimate ASI Loader (or any ASI loader) next to
//    CLEO, SilentPatch and other mods. The DirectInput8Create export below is
//    simply unused in that case.
//  * dinput8.dll   - standalone, when no ASI loader is installed: the game loads
//    it in place of the system dinput8.dll and every call is forwarded.
#include <windows.h>
#include <stdio.h>
#include "trainer.h"

typedef HRESULT (WINAPI* DI8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

// The asm label gives the export an undecorated name (no @20) so it matches
// the "DirectInput8Create" import in gta-vc.exe.
extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN)
    __asm__("_DirectInput8Create");
extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE inst, DWORD ver, REFIID iid, LPVOID* out, LPUNKNOWN outer) {
    static DI8Create_t real = nullptr;
    if (!real) {
        char path[MAX_PATH];
        GetSystemDirectoryA(path, MAX_PATH);   // SysWOW64 for this 32-bit process
        lstrcatA(path, "\\dinput8.dll");
        if (HMODULE h = LoadLibraryA(path)) real = (DI8Create_t)GetProcAddress(h, "DirectInput8Create");
        if (!real) return E_FAIL;
    }
    return real(inst, ver, iid, out, outer);
}

// Only one copy of the trainer may hook the game, even if it is installed
// twice (e.g. VCTrainer.asi and an old dinput8.dll copy of it).
static bool FirstInstance() {
    char name[64];
    sprintf(name, "Local\\VCTrainer.%lu", GetCurrentProcessId());
    HANDLE m = CreateMutexA(nullptr, FALSE, name);   // kept open for the process lifetime
    return m && GetLastError() != ERROR_ALREADY_EXISTS;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        if (!FirstInstance()) return TRUE;           // another copy is already running
        if (!InstallHooks())
            MessageBoxA(nullptr, "Modern VC Trainer: this gta-vc.exe is not v1.0. The trainer is disabled.",
                        "Modern VC Trainer", MB_ICONWARNING);
    }
    return TRUE;
}
