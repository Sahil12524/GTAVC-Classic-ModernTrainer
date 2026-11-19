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
    bool bhopInstant = false;     // hop at top speed straight away, no build-up
    bool bhopKeepSpeed = false;   // walls never slow a hop: slide along / bounce off at full speed
    bool noLandingRoll = false;   // no roll / crouch / get-up when landing from a fall
    float superRunSpeed = 2.5f;   // times normal speed       // guns fire as fast as the button allows
    bool noRagdoll = false;       // the player is never knocked over
    bool walkOnWater = false;     // on foot, water is solid ground: walk, run, jump, land on it
    bool wallBang = false;        // shots go through walls, cars, wrecks
    bool oneShotVehicles = false; // any vehicle shot is destroyed (tanks, helicopters...)
    bool bulletPenetrate = false; // any gun: bullets pass through walls, people and cars, reach 500 m
    bool enemyEsp = false;    // mark every hostile ped on screen, walls or not
    bool aimLock = false;     // aiming near an enemy who wants the player dead: crosshair locks on
    bool aimSilent = false;   // aim lock: camera never turned, shots land on the target anyway
    bool aimSilentBhop = true; // aim lock: silent aim while bunny hopping (turning the camera turns the run)
    bool aimSnapBack = true;  // aim lock: camera returns to the player's view when the lock ends
    bool aimDriveBy = false;  // drive-by shots from a vehicle land on enemies (follows Silent aim)
    bool touchKill = false;   // peds die the moment the player (or the player's vehicle) touches them
    bool autoLock = false;    // lock doors on getting in (cars with doors), unlock on getting out
    bool autoFlip = false;    // cars and boats: turn back over when upside down
    bool heliAimbot = false;  // Hunter / Sea Sparrow: firing kills the marked targets (vigilante)
    // extras (Menyoo-style)
    bool invisible = false, freezePos = false, forcefield = false;
    bool explosiveBullets = false, fireBullets = false, superPunch = false;
    bool teleportGun = false, deleteGun = false, vehicleGun = false;
    bool rainbowCar = false, bulletproofTyres = false, invisibleCar = false;
    bool bulletTime = false, hideRadar = false;
    int  forcefieldMode = 0;                      // 0 push, 1 destroy (cars blow up, people die)
    float damageMult = 1.0f;                      // player's guns
    float carAccel = 1.0f, carGrip = 1.0f, carBrakes = 1.0f;
    float gravity = 1.0f, clockSpeed = 1.0f;
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
    bool overlayShowPos = false;  // CS:GO cl_showpos style: pos / ang / vel, top left
    int  showPosUnits = 0;        // 0 u/s (CS:GO units), 1 km/h, 2 m/s
    float fov = 70.0f;            // field of view, degrees (70 = the game's own)
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
// ---- extras
struct PedModel {
    int id; std::string name;
    std::string txd, type, stat, anim;                // default.ide: ped type (CIVMALE, COP, GANG1...), stats, walk style
    unsigned cars = 0;                                // cars-can-drive mask (hex)
    int radio1 = 9, radio2 = 9;                       // favourite radio stations (9 = none)
};
const std::vector<PedModel>& PedModels();          // data\default.ide "peds"
struct PedStat { float flee = 0, heading = 0; int fear = 0, temper = 0, law = 0, sexy = 0; float attack = 1, defend = 1; int flags = 0; };
const PedStat* PedStatOf(const std::string& statName);   // data\pedstats.dat, by STAT_ name
bool SetPlayerSkin(const char* name, bool quiet = false);   // outfit or ped model (UNDRESS / DRESS_CHAR); false = not changed
std::string CurrentPlayerSkin();                   // the look the player wears now, as SetPlayerSkin takes it
void RemoveAllWeapons();
void SpawnBodyguards(int model, int count, int weapon);
void DismissBodyguards();
int  BodyguardCount();
void SpawnPed(int model, bool hostile, int weapon);
void RemoveSpawnedPeds();
enum { NEARBY_FIRE, NEARBY_FREEZE, NEARBY_UNFREEZE, NEARBY_RIOT, NEARBY_DELETE, NEARBY_RECRUIT };
void NearbyPeds(int action);
void DeleteVehicle();
struct GarageCar { int model, c1, c2; std::string name; };
std::vector<GarageCar>& Garage();
void GarageSave(const char* name);
void GarageSpawn(int index);
void GarageRemove(int index);
struct ShowPos { float pos[3], ang[3], vel; };
bool GetShowPos(ShowPos& out);   // cl_showpos numbers for the overlay (vel in the chosen units)

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
struct VehicleEntry { int model; int category; std::string name; std::string handling, cls; };   // default.ide handling id, traffic class
// data\handling.cfg, one vehicle's main line
struct VehicleSpec {
    float mass = 0, traction = 0, maxKmh = 0, accel = 0, brake = 0;
    int gears = 0, value = 0;
    char drive = 0, engine = 0;                       // F / R / 4,  P / D / E
    unsigned flags = 0;
};
const VehicleSpec* VehicleSpecOf(const std::string& handlingId);
enum { CAT_CARS, CAT_BIKES, CAT_BOATS, CAT_HELIS, CAT_RC, CAT_COUNT };
const std::vector<VehicleEntry>& VehicleList(); // built from data\default.ide + game text
void SpawnVehicle(int model);
// vehpreview.cpp: still pictures of vehicles, rendered from the game's own models
void* VehiclePicture(int model);        // the picture (IDirect3DTexture8*), or null while it's made (asks for it)
bool VehiclePictureMissing(int model);  // nothing could be drawn for this model
void VehiclePicturesPump();             // once per frame after the tiles: makes the next picture
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
const std::vector<EnemyMark>& EnemyMarks();    // empty unless "show enemies" is on; valid until the next call
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
const std::vector<ActiveItem>& ActiveCheats();   // cached; rebuilt only when something on it changed
void MarkActiveDirty();                           // a switch changed: rebuild the Active list
void TurnEverythingOff();
