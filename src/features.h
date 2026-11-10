// Trainer feature state and actions. Actions requested from the menu are
// queued and executed at the start of the next game frame (game thread, inside
// CGame::Process), never in the middle of rendering.
#pragma once
#include <functional>
#include <string>
#include <vector>

struct Proofs { bool bullet = false, fire = false, explosion = false, collision = false, melee = false; };

struct Settings {
    bool enabled = true;      // master switch: when off, every continuous cheat pauses
    // player
    bool godMode = false, infiniteArmour = false, neverWanted = false;
    bool infiniteAmmo = false, noReload = false;
    Proofs playerProofs;
    // vehicle
    bool carGodMode = false, autoRepair = false;
    bool heavyCar = false;
    int  heavyMode = 1;       // what touching another car does: 0 heavy only, 1 push far away, 2 blow up
    Proofs carProofs;
    bool neverTired = false, fastReload = false, stayOnBike = false;
    bool flyMode = false;
    float flySpeed = 25.0f;   // metres per second, x3 while Shift is held
    bool warpIntoSpawned = true;
    // world
    bool freezeTime = false, freezeMissionTimer = false;
    int  frozenHour = 12, frozenMinute = 0;
    // overlay
    bool overlayCoords = false, overlaySpeed = false, overlayFps = false, overlayActive = false;
};
extern Settings S;
// The settings that are in effect right now: S, or all-off while the master
// switch is off (S itself is kept so switching back on restores everything).
const Settings& Active();
void SetTrainerEnabled(bool on);

// Configurable hotkeys (work while the menu is closed). Every trainer feature
// is registered here: toggles flip a setting, presses run once, holds run every
// frame the key is down.
enum { HKMOD_CTRL = 1, HKMOD_SHIFT = 2, HKMOD_ALT = 4 };
struct HotkeyAction {
    enum Kind { Press, Toggle, Hold };
    std::string id, label, category;
    Kind kind;
    bool* toggle;                 // Toggle
    std::function<void()> fn;     // Press / Hold
    int vk = 0, mods = 0;
    bool prev = false;
};
std::vector<HotkeyAction>& HotkeyActions();
extern int g_capturingHotkey;   // index into HotkeyActions() being rebound, -1 if none

int  CurrentModifiers();                      // HKMOD_* bits currently held
bool IsModifierKey(int vk);
void BindCapturedKey(int vk, int mods);       // finishes a capture (vk 0 = clear)
void HotkeyName(int vk, int mods, char* out, int size);

void LoadSettings();
void SaveSettings();
bool OverlayEnabled();

void Queue(std::function<void()> fn);

// info for the menu
void* PlayerPed();
void* PlayerVehicle();
bool  GetPlayerPos(float out[3]);

// player
void SetMaxHealth(int v);
void SetMaxArmour(int v);
void SetHealth(float v);
void SetArmour(float v);
void SetWanted(int level);
int  GetWanted();
void AddMoney(int amount);
void SetMoney(int amount);
void GiveWeapon(int type, int ammo);

// vehicle
struct VehicleEntry { int model; int category; std::string name; };
enum { CAT_CARS, CAT_BIKES, CAT_BOATS, CAT_HELIS, CAT_RC, CAT_COUNT };
const std::vector<VehicleEntry>& VehicleList(); // built from data\default.ide + game text
void SpawnVehicle(int model);
void WarpIntoNearestVehicle();
int  LastSpawnedModel();
void SetVehicleColours(int c1, int c2);
bool GetVehicleColours(int& c1, int& c2);
struct PaletteColour { unsigned char r, g, b; std::string name; };
const std::vector<PaletteColour>& CarPalette();   // data\carcols.dat
void JumpVehicle(float amount);
float VehicleSpeedKmh();
void RepairVehicle();
void FlipVehicle();
void SetDoorLock(bool locked);
void BoostVehicle(float amount);
void StopVehicle();

// world
void SetTime(int h, int m);
void ForceWeather(int type);
void ReleaseWeather();

// teleport
struct SavedPos { std::string name; float x, y, z; };
std::vector<SavedPos>& SavedPositions();
void SavePositions();
void TeleportTo(float x, float y, float z, bool findGround);
struct Blip { int index, type, sprite; float pos[3]; };
std::vector<Blip> ActiveBlips();             // radar blips with a resolvable position
bool TeleportToMarker();                     // first mission marker (no icon), else any blip
void TeleportToBlip(const Blip& b);

// native cheats
void RunNativeCheat(const char* code);
bool NativeCheatHasState(const char* code);   // has an on/off switch the trainer can read
bool NativeCheatIsOneWay(const char* code);   // can't be undone without reloading
bool NativeCheatOn(const char* code);
void SetNativeCheat(const char* code, bool on);
const char* NativeCheatLabel(const char* code);

// everything currently switched on, for the Active list and overlay
struct ActiveItem { std::string label, detail; std::function<void()> turnOff; };
std::vector<ActiveItem> ActiveCheats();
void TurnEverythingOff();
