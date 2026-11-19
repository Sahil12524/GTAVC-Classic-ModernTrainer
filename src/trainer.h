#pragma once

#define VCTRAINER_VERSION "1.3.0"

void Log(const char* fmt, ...);               // log.cpp: one line in VCTrainer.log (next to gta-vc.exe)
void GamePath(char* out, const char* name);   // log.cpp: name in the game folder (out: MAX_PATH chars)

extern bool  g_menuOpen;
extern float g_mouseSensitivity;

bool InstallHooks();               // hooks.cpp
void ForceOffBike(void* bike, void* ped); // hooks.cpp, game's own knock-off, no re-seat
void BikeKnockedOff(void* bike);          // features.cpp, called by the knock-off hook
bool BikeExitRequested();                 // exit pressed on a bike in the last 2 s
bool HoldNoWanted();
int WantedCap();                        // features.cpp: highest stars allowed, -1 = no limit
// player.cpp: stars the player picked above the story's limit (0 = none). While
// set, the wanted hook lifts the game's limit for the player's own wanted level
// only, and puts it back straight after (the limit itself is saved in saves).
extern int g_wantedAboveLimit;
struct WantedLimitLift {
    int savedLevel = 0, savedChaos = 0; bool on = false;
    explicit WantedLimitLift(bool lift);
    ~WantedLimitLift();
};
void NoteCarExitFall();                // features.cpp: a car-exit knock-down is playing, leave it
bool NoRagdollOn();                     // features.cpp: player can't be knocked down
bool TrainerWeaponFire(void* w, void* shooter, float* source, bool (__thiscall* orig)(void*, void*, float*)); // features.cpp: every CWeapon::Fire
void TrainerDriveByShot(void* w, void* veh, int left, int right); // combat.cpp: the game's drive-by FireFromCar just ran
void TrainerHeliShot(void* w, void* shooter, const float* source, bool ok); // combat.cpp: a Hunter rocket / cannon shot just ran
void ApplyPendingCameraTurns();     // combat.cpp: the aim locks' turns, from the pad update (before the camera is built)
// combat.cpp: the crosshair ray at the drawn (widened) FOV; false = let the game's own run
bool WideCrosshairTarget(float dist, const float* pos, float* source, float* target);
extern bool g_wideCrosshairHook;    // hooks.cpp: Find3rdPersonCamTargetVector is hooked (aiming may be widened)
bool SurviveBlowUpOn();                   // features.cpp: explosion hook may take the player out
void EjectPlayerFromBlast(void* veh);     // features.cpp: player out of an exploding vehicle, alive
bool BlowUpStayInside();                  // features.cpp: survive mode 0 = stay in the wreck
void** PlayerSeatInBlast(void* veh);      // features.cpp: the player's seat slot in veh (protects the player)                      // features.cpp: wanted hook keeps the player at 0 stars
void ApplyTrainerStyle();          // menu.cpp
void LoadUiFonts();                // menu.cpp: the menu fonts (from the UI settings)
void UiBeforeNewFrame();           // menu.cpp: font changes, applied between frames
void DrawTrainerMenu();            // menu.cpp (render thread = game thread)
void TrainerTick();                // features.cpp, once per game frame
void DoorLockBeforeInput();        // features.cpp, right after the pads update (before the game reads them)
void ShowNative(const char* fmt, ...); // features.cpp, game's own help box
// Hotkeys: presses are recorded as they happen and run on the next tick, so a
// tap shorter than a tick still counts.
void NoteKeyPress(int vk, int mods);   // features.cpp: from the window procedure (key down, not a repeat)
void PollMouseHotkeys();               // features.cpp: once per drawn frame, mouse button presses
bool AnyOverlayOn();                   // features.cpp: overlay or enemy markers want drawing with the menu closed
// ESP scan cost on the tick since the last call (for the timing line in the log)
void TakeEspScanStats(double* totalMs, int* scans, int* marks);
