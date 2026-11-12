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
    bool wantedCapOn = false;     // the wanted level never goes above wantedCap
    int  wantedCap = 2;           // 0-6 stars
    int  maxHealth = 0, maxArmour = 0;   // chosen max health/armour, 0 = game's own
    bool infiniteAmmo = false, noReload = false;
    Proofs playerProofs;
    // vehicle
    bool carGodMode = false, autoRepair = false;
    bool heavyCar = false;
    bool genocide = false;        // every ped and vehicle around dies / blows up
    bool rapidFire = false;
    bool noScopeSniper = false;   // sniper rifles fire from the hip with one click
    bool sniperAuto = false;      // hold the button: sniper rifles keep firing at full speed
    bool superJump = false;       // jumps go much higher
    float superJumpPower = 3.0f;  // times a normal jump
    bool superRun = false;        // run / sprint much faster on foot
    bool autoBhop = false;        // hold jump: hop again the moment you land, gaining speed
    int  bhopMode = 0;            // 0 any direction, 1 AFH (forward), 2 ABH (backward)
    float bhopGain = 1.0f;        // speed kept x this each hop (1 = CS:GO)
    float bhopMax = 89.0f;        // top hop speed, m/s (CS:GO sv_maxvelocity 3500 u/s)
    float bhopHeight = 1.45f;     // hop height, metres (CS:GO: 57 units)
    bool bhopSurf = true;         // slide along steep ramps like CS:GO surf
    bool noLandingRoll = false;   // no roll / crouch / get-up when landing from a fall
    float superRunSpeed = 2.5f;   // times normal speed       // guns fire as fast as the button allows
    bool noRagdoll = false;       // the player is never knocked over
    bool wallBang = false;        // shots go through walls, cars, wrecks
    bool oneShotVehicles = false; // any vehicle shot is destroyed (tanks, helicopters...)
    bool enemyEsp = false;    // mark every hostile ped on screen, walls or not
    bool aimLock = false;     // aiming near an enemy who wants the player dead: crosshair locks on
    bool touchKill = false;   // peds die the moment the player (or the player's vehicle) touches them
    bool autoLock = false;    // lock doors on getting in (cars with doors), unlock on getting out
    bool autoFlip = false;    // cars only: turn back over when resting on the roof
    bool surviveBlowUp = true; // vehicle explodes with the player inside: the player lives
    int  blowUpMode = 0;       // 0 stay in the wreck, 1 thrown clear beside it
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
void KillPlayer();                    // suicide that works through god mode, proofs, vehicles
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
void TeleportTo(float x, float y, float z, bool findGround, bool clearSpace = true,
                void* ignore = nullptr, const float* heading = nullptr);
struct Blip { int index, type, sprite; float pos[3]; };
std::vector<Blip> ActiveBlips();             // radar blips with a resolvable position
bool TeleportToMarker();
struct EnemyMark {
    bool onScreen, visible, locked;
    float x, top, bottom;     // screen pixels (game resolution) when on screen
    float dirX, dirY;         // off screen: right / ahead of the camera, metres
    float dist;
};
std::vector<EnemyMark> EnemyMarks();           // empty unless "show enemies" is on
struct Pickup { int index, model; float pos[3]; };
std::vector<Pickup> ActivePickups();           // pickups lying in the world (packages, rampages, ...)
void TeleportToPickup(const Pickup& p);                     // first mission marker (no icon), else any blip
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
