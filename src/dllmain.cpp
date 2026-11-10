// dinput8.dll proxy: forwards DirectInput8Create to the real system DLL and
// installs the trainer hooks when the game loads us.
#include <windows.h>
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

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        if (!InstallHooks())
            MessageBoxA(nullptr, "VC Trainer: this gta-vc.exe is not v1.0. The trainer is disabled.",
                        "VC Trainer", MB_ICONWARNING);
    }
    return TRUE;
}
