// GTA Vice City v1.0 (gta-vc.exe, 3,088,896 bytes) addresses.
// Every address here was located and checked in the disassembly of this exe;
// the comment says where it came from.
#pragma once
#include <windows.h>

namespace game {

// ---- functions -------------------------------------------------------------
using FnPtr0      = void*(__cdecl*)();
using SetHelp_t   = void (__cdecl*)(const wchar_t*, bool, bool);
using TextGet_t   = const wchar_t* (__thiscall*)(void* text, const char* key);
using SetWanted_t = void (__thiscall*)(void* wanted, int level);
using GiveWeap_t  = void (__thiscall*)(void* ped, int type, int ammo, bool unused);
using WeapInfo_t  = char* (__cdecl*)(int type);
using ReqModel_t  = void (__cdecl*)(int model, int flags);
using LoadAll_t   = void (__cdecl*)(bool onlyPriority);
using ModelFn_t   = void (__cdecl*)(int model);
using New_t       = void* (__cdecl*)(unsigned size);
using VehCtor_t   = void* (__thiscall*)(void* mem, int model, int createdBy);
using WorldAdd_t  = void (__cdecl*)(void* entity);
using Weather_t   = void (__cdecl*)(int type);
using SetClock_t  = void (__cdecl*)(unsigned char h, unsigned char m);
using LoadScene_t = void (__cdecl*)(const float* pos);
using GroundZ_t   = float (__cdecl*)(float x, float y);
using CheatChar_t = void (__thiscall*)(void* pad, char c);

inline auto FindPlayerPed      = (FnPtr0)      0x4BC120;
inline auto FindPlayerVehicle  = (FnPtr0)      0x4BC1E0;
inline auto SetHelpMessage     = (SetHelp_t)   0x55BFC0; // CHud::SetHelpMessage
inline auto TextGet            = (TextGet_t)   0x584F30; // CText::Get
inline auto SetWantedLevel     = (SetWanted_t) 0x4D1F80; // CWanted::SetWantedLevel
inline auto GiveWeapon         = (GiveWeap_t)  0x4FFA30; // CPed::GiveWeapon
inline auto GetWeaponInfo      = (WeapInfo_t)  0x5D5710; // CWeaponInfo::GetWeaponInfo
inline auto RequestModel       = (ReqModel_t)  0x40E310; // CStreaming::RequestModel
inline auto LoadAllRequested   = (LoadAll_t)   0x40B5F0; // CStreaming::LoadAllRequestedModels
inline auto SetModelDeletable  = (ModelFn_t)   0x40AEA0;
inline auto SetModelTxdDeletable = (ModelFn_t) 0x40ADF0;
inline auto VehicleNew         = (New_t)       0x5BAB20; // CVehicle::operator new
inline auto AutomobileCtor     = (VehCtor_t)   0x59E620; // size 0x5DC
inline auto BikeCtor           = (VehCtor_t)   0x615740; // size 0x4EC
inline auto BoatCtor           = (VehCtor_t)   0x5A6470; // size 0x4C0
inline auto WorldAdd           = (WorldAdd_t)  0x4DB3F0; // CWorld::Add
// Full repair, as the game's own "fix car" code does it (health = 1000, fire
// timer cleared, then Fix): CAutomobile::Fix resets the whole damage manager
// and puts every bent/hanging panel back; CBike::Fix re-inflates the tyres.
inline auto AutomobileFix      = (void(__thiscall*)(void*))0x588530;
inline auto BikeFix            = (void(__thiscall*)(void*))0x609F00;
constexpr int AUTO_FIRE_TIMER = 0x560, BIKE_FIRE_TIMER = 0x494;
constexpr int BIKE_TYRES = 0x32C;            // 2 bytes, front/rear, 0 = ok
// peds getting in (counter, inc/dec by the enter code), door bits of peds
// getting in / getting out - all 0 when nobody is using a door
constexpr int VEH_NUM_GETTING_IN = 0x1CD, VEH_GETTING_IN_FLAGS = 0x1CE, VEH_GETTING_OUT_FLAGS = 0x1CF;
constexpr int VEH_NUM_PASSENGERS = 0x1CC, VEH_MAX_PASSENGERS = 0x1D0;   // AddPassenger 0x5B8850 / ctor 0x5BAEDC
constexpr int MODEL_AMBULANCE = 146;
// CWorld::FindObjectsKindaColliding(pos, radius, checkTop, &count, max, out, buildings, vehicles, peds, objects, dummies)
inline auto FindObjectsKindaColliding = (void(__cdecl*)(const float*, float, bool, short*, short, void**,
                                                        bool, bool, bool, bool, bool))0x4D6370;
// CWorld::GetIsLineOfSightClear(a, b, buildings, vehicles, peds, objects, dummies, seeThrough, ignoreSomeObjects)
inline auto LineOfSightClear = (bool(__cdecl*)(const float*, const float*, bool, bool, bool, bool, bool,
                                               bool, bool))0x4DA560;
// CWaterLevel::GetWaterLevelNoWaves(x, y, z, &level): false = no water there
inline auto WaterLevelAt = (bool(__cdecl*)(float, float, float, float*))0x5C2BE0;
constexpr int MI_COLMODEL = 0x1C, COL_RADIUS = 0x0C, COL_BOX_MIN = 0x14, COL_BOX_MAX = 0x20;
constexpr int VEH_FLAGS_1FB = 0x1FB;
// --- taking a ped out of a vehicle, as script command 0362 (WARP_CHAR_FROM_CAR_TO_COORD) does
constexpr int PED_OBJECTIVE = 0x164, PED_CAR_IN_OBJECTIVE = 0x170;   // set by CPed::SetObjective
constexpr int VEH_DRIVER = 0x1A8;   // CPed* pDriver (passengers follow at 0x1AC)
constexpr int PED_MY_VEHICLE = 0x3A8, PED_IN_VEHICLE_FLAG = 0x3AC, PED_VEHICLE_ANIM = 0x1F8;
constexpr int PED_ANIM_GROUP = 0x1F4, ENT_CLUMP = 0x4C, VEH_CRUISE_SPEED = 0x160;
inline auto VehRemoveDriver    = (void(__thiscall*)(void*))0x5B8920;
inline auto VehRemovePassenger = (void(__thiscall*)(void*, void*))0x5B8CE0;
inline auto Ped_512440         = (void(__thiscall*)(void*, int))0x512440;   // called with 1 first
inline auto Ped_4F7920         = (void(__thiscall*)(void*))0x4F7920;        // only when state == 0xB
inline auto Ped_4FF5A0         = (void(__thiscall*)(void*))0x4FF5A0;        // weapon model back in hand
inline auto Ped_50D110         = (void(__thiscall*)(void*, int))0x50D110;   // restart anims (arg 0)
inline auto Ped_50CCF0         = (void(__thiscall*)(void*))0x50CCF0;
inline auto PedDistToBase      = (float(__thiscall*)(void*))0x487D10;      // GetDistanceFromCentreOfMassToBaseOfModel
inline auto BlendAnimation     = (void*(__cdecl*)(void*, int, int, float))0x405640;
inline auto ClearSpaceForMissionEntity = (void(__cdecl*)(const float*, void*))0x45F180;          // bit 0x02 = bIsDamaged
constexpr int VEH_HANDLING = 0x120, HANDLING_FLAGS = 0xCC, HANDLING_NO_DOORS = 0x10;
// CDamageManager (at VEH_DAMAGE): engine +4, wheels +5..8, doors +9..0xE,
// lights dword +0x10, panels dword +0x14.
inline auto ForceWeatherNow    = (Weather_t)   0x57D550; // CWeather::ForceWeatherNow
inline auto SetGameClock       = (SetClock_t)  0x487160; // CClock::SetGameClock
inline auto LoadScene          = (LoadScene_t) 0x40AF60; // CStreaming::LoadScene
// Collision at a spot, as the game's script teleport does before LoadScene:
// CColStore (x,y) then request-and-load the collision (RequestCollision + LoadAllRequestedModels).
inline auto ColStoreLoadAt     = (void(__cdecl*)(const float* xy))0x62A140;
inline auto LoadSceneCollision = (void(__cdecl*)(const float* pos))0x40AF30;
inline auto FindGroundZ        = (GroundZ_t)   0x4D5540; // CWorld::FindGroundZForCoord
// CWorld::FindGroundZFor3DCoord(x, y, z, bool* found): ground below a point,
// traced down from z (script opcode 02CE), so roofs/bridges above don't count.
inline auto FindGroundZ3D = (float(__cdecl*)(float, float, float, bool*))0x4D53A0;
inline auto AddToPCCheatString = (CheatChar_t) 0x4ABD20; // CPad::AddToPCCheatString

using PoolGet_t   = void* (__thiscall*)(void* pool, int handle);
using SetObj_t    = void (__thiscall*)(void* ped, int objective, void* target);
using Warp_t      = void (__thiscall*)(void* ped, void* vehicle);
inline auto PoolGetVehicle     = (PoolGet_t)   0x451C70; // CPool<CVehicle>::GetAt
inline auto PoolGetPed         = (PoolGet_t)   0x451CB0; // CPool<CPed>::GetAt
inline auto PoolGetObject      = (PoolGet_t)   0x451C30; // CPool<CObject>::GetAt
inline auto PedSetObjective    = (SetObj_t)    0x521F10; // CPed::SetObjective(obj, entity)
inline auto WarpPedIntoCar     = (Warp_t)      0x4EF8B0; // CPed::WarpPedIntoCar (opcode 0369)

// ---- data ------------------------------------------------------------------
inline void* const TheText      = (void*)0x94B220;
inline void* const Pad0         = (void*)0x7DBCB0;          // CPad::Pads[0]
inline int*  const Money        = (int*)0x94ADC8;           // Players[0].m_nMoney
inline unsigned char* const MaxHealth = (unsigned char*)0x94AE6B;
inline unsigned char* const MaxArmour = (unsigned char*)0x94AE6C;
inline void** const D3DDevice   = (void**)0x7897A8;         // RwD3D8 device
inline HWND* const GameWindow   = (HWND*)0x7897A4;
inline unsigned char* const MouseState = (unsigned char*)0x7DD860; // CPad::NewMouseControllerState
inline unsigned char* const InvertMouseX = (unsigned char*)0xA10A4C;
inline unsigned char* const InvertMouseY = (unsigned char*)0xA10A4D;
inline unsigned char* const ClockHours   = (unsigned char*)0xA10B6B;
inline unsigned char* const ClockMinutes = (unsigned char*)0xA10B92;
// CClock::Update (0x487010) adds a minute once TimeMs - LastClockTick passes a
// game minute, and recomputes the seconds every frame from that difference.
inline unsigned* const ClockLastTick      = (unsigned*)0xA0FDEC;        // CClock::ms_nLastClockTick
inline unsigned short* const ClockSeconds = (unsigned short*)0xA10A3C;  // CClock::ms_nGameClockSeconds
inline unsigned* const TimeMs             = (unsigned*)0x974B2C;        // CTimer::m_snTimeInMilliseconds
// CWeather::Update (0x57D580): interp = (seconds/60 + minutes)/60; when it is
// lower than last frame's value it treats that as a new hour and moves on to
// the next weather in the list.
inline float* const WeatherInterp = (float*)0x9787D8;               // CWeather::InterpolationValue
inline short* const ForcedWeather = (short*)0xA10A42;       // -1 = not forced
inline float* const TimeScale     = (float*)0x97F264;       // CTimer::ms_fTimeScale
inline float* const PedDensity    = (float*)0x694DC0;
inline float* const CarDensity    = (float*)0x686FC8;
inline char*  const ScriptSpace   = (char*)0x821280;
// CDarkel (rampages): Update 0x42A650 - remaining = TimeLimit - (now - start)
inline short* const FrenzyStatus     = (short*)0xA10A72;     // 1 = rampage running
inline int* const FrenzyTimeLimit    = (int*)0x974BE0;       // ms, < 0 = no limit
inline unsigned* const FrenzyStart   = (unsigned*)0xA0FDE0;  // TimeMs when it started
// CPickups::aPickUps[336], 0x34 each (GenerateNewOne 0x441A..)
inline char* const PickupArray = (char*)0x945D30;
constexpr int PICKUP_COUNT = 336, PICKUP_SIZE = 0x34;
constexpr int PICKUP_MODEL = 0x22, PICKUP_TYPE = 0x2E, PICKUP_REMOVED = 0x2F;
// CPed flag byte: bit 0 bIsStanding, bit 1 bWasStanding (cleared before SetFall 0x4FD9F0)
constexpr int PED_FLAGS_14C = 0x14C, PED_FALL = 0x2A, PED_GETUP = 0x2B, PED_IDLE = 1;
inline unsigned* const OnscnTimerOffset = (unsigned*)0x9B6CF8; // CUserDisplay::OnscnTimer entry 0
inline void** const ModelInfoPtrs = (void**)0x92D4C8;       // CModelInfo::ms_modelInfoPtrs
inline unsigned char* const StreamInfo = (unsigned char*)0x94DDD8; // ms_aInfoForModel, stride 20
inline unsigned char* const NeverTired = (unsigned char*)0x94AE68; // Players[0]+0x140 (opcode 0330)
inline unsigned char* const FastReload = (unsigned char*)0x94AE69; // Players[0]+0x141 (opcode 0331)
inline void** const VehiclePool = (void**)0xA0FDE4;         // CPools::ms_pVehiclePool
inline void** const PedPool     = (void**)0x97F2AC;         // CPools::ms_pPedPool
inline void** const ObjectPool  = (void**)0x94DBE0;         // CPools::ms_pObjectPool
inline char* const RadarTrace   = (char*)0x7D7D38;          // CRadar::ms_RadarTrace[75]
// TheCamera's matrix: right +0, look direction +0x10, up +0x20, position +0x30
// (heading code passes 0x7E4698/0x7E469C to GetATanOfXY; pos 0x7E46B8 used as camera x,y).
inline float* const CameraFront = (float*)0x7E4698;
inline float* const CameraRight = (float*)0x7E4688;
inline float* const CameraPos   = (float*)0x7E46B8;
// CSprite::CalcScreenCoors(in, out (pixels; z = depth), &w, &h, farClip): false = behind the camera
inline auto CalcScreenCoors = (bool(__cdecl*)(const float*, float*, float*, float*, bool))0x5778B0;
inline int* const ScreenWidth  = (int*)0x9B48E4;   // RsGlobal.maximumWidth / Height
inline int* const ScreenHeight = (int*)0x9B48E8;
inline void* const TheCamera = (void*)0x7E4688;
// CCamera::SetCameraDirectlyBehindForFollowPed_CamOnAString (script opcode 0373)
inline auto CameraBehindPlayer = (void(__thiscall*)(void*))0x46BADE;
// CPad::UpdateMouse copies the raw mouse (MouseState, 0x7DD860) into this
// "current" state, which is what the camera and weapons actually read.
// The camera: CCamera::ActiveCam (byte) +0x76, Cams[3] of 0x1CC from +0x188;
// each CCam: Mode (short) +0x0C, Alpha (up/down) +0xAC, Beta (round) +0xBC -
// what mouse look changes (CCam::Process_FollowPed... 0x479632).
inline char* ActiveCam() { return (char*)0x7E4810 + *(unsigned char*)0x7E46FE * 0x1CC; }
constexpr int CAM_MODE = 0x0C, CAM_ALPHA = 0xAC, CAM_BETA = 0xBC;
// CCamera::Find3rdPersonCamTargetVector(range, source xyz, &camSource, &camTarget): where the crosshair points
inline auto Find3rdPersonCamTargetVector = (void(__thiscall*)(void*, float, float, float, float, float*, float*))0x46F890;
constexpr int PED_ATTACK = 0x10, PED_FIGHT = 0x11, PED_AIM_GUN = 0x16;   // SetAttack 0x52D406, SetFight 0x52ABC4
// CHeli (police helicopter, vtable 0x69C21C, its BlowUpCar is empty):
// CHeli::pHelis[4]; status +0x2C0 (3 = shot down, TestBulletCollision
// 0x5AC66D), explosion time +0x2CC.
inline void** const HeliList = (void**)0x813D10;
constexpr int HELI_COUNT = 4, HELI_STATUS = 0x2C0, HELI_EXPLODE_AT = 0x2CC, HELI_SHOT_DOWN = 3;
// CPedType::ms_apPedType[type]: m_threats +0x18, bit 0 = PLAYER1 ("hates the
// player"). Riot / peds-attack cheats set it on every type (0x4AC2A1).
inline void** const PedTypeTable = (void**)0xA0DA64;
constexpr int PEDTYPE_THREATS = 0x18, PEDTYPE_COUNT = 23, THREAT_PLAYER1 = 1;
constexpr int PED_JUMP = 0x29;
// RpAnimBlendClumpGetAssociation(clump, animId) (0x407780, as SetJump uses it)
inline auto GetAnimAssoc = (char*(__cdecl*)(void* clump, int animId))0x407780;
// CAnimBlendAssociation: blendAmount +0x18, blendDelta +0x1C, animId +0x2C, flags +0x2E (4 = delete when faded)
constexpr int ASSOC_BLEND = 0x18, ASSOC_DELTA = 0x1C, ASSOC_FLAGS = 0x2E, ASSOCF_DELETEFADEDOUT = 0x04;
// ped anim ids (CAnimManager ped table 0x683CA8)
constexpr int ANIM_JUMP_LAUNCH = 0x8D, ANIM_JUMP_GLIDE = 0x8E, ANIM_IDLE_STANCE = 0x03;
constexpr int ANIM_GETUP1 = 0x89, ANIM_GETUP_FRONT = 0x8C, ANIM_JUMP_LAND = 0x8F, ANIM_FALL_LAND = 0x92,
              ANIM_FALL_COLLAPSE = 0x93;
constexpr int PAD_JUMP = 0x1C;
// CWorld::ProcessLineOfSight(a, b, CColPoint&, CEntity*&, buildings, vehicles, peds, objects, dummies,
// seeThrough, ignoreSomeObjects, shootThrough) (0x4D92D0); CColPoint: point +0, normal +0x10
inline auto ProcessLineOfSight = (bool(__cdecl*)(const float*, const float*, void*, void**, bool, bool, bool, bool,
                                                 bool, bool, bool, bool))0x4D92D0;
constexpr int COLPOINT_NORMAL = 0x10;   // CPad NewState.ButtonSquare (jump)   // state written by CPed::SetJump (0x4F054E)
constexpr int PED_TYPE = 0x3D4;   // 6 cop, 7-15 gangs, 16 emergency, 17 fireman, 18 criminal
constexpr int OBJ_FOLLOW_CHAR_IN_FORMATION = 15;
inline unsigned char* const GameMenuActive = (unsigned char*)0x869668;   // FrontEndMenuManager.m_bMenuActive
constexpr int PED_PED_IN_OBJECTIVE = 0x16C, OBJ_KILL_CHAR_ON_FOOT = 8, OBJ_KILL_CHAR_ANY_MEANS = 9;
inline unsigned char* const NewMouseState = (unsigned char*)0x94D788;

// CBike::KnockOffRider(weaponType, direction, ped, getBackOn) - thiscall on the
// bike, ret 0x10. Called on crashes, hits, falls; 14 call sites.
constexpr DWORD KNOCK_OFF_RIDER = 0x613920;
constexpr int KNOCK_OFF_PROLOGUE = 10; // push ebx/esi/edi/ebp + sub esp,0xD0
constexpr int RADAR_COUNT = 75, RADAR_SIZE = 0x38;
constexpr int BLIP_TYPE = 0x04, BLIP_HANDLE = 0x08, BLIP_POS = 0x18, BLIP_INUSE = 0x27, BLIP_SPRITE = 0x34;
constexpr int POOL_OBJECTS = 0x0, POOL_FLAGS = 0x4, POOL_SIZE = 0x8, VEHICLE_POOL_STRIDE = 0x5DC;
constexpr int PED_POOL_STRIDE = 0x6D8;   // sizeof(CPlayerPed) slot in CPool<CPed> (GetAt 0x451CB0)

// ---- object offsets ----------------------------------------------------------
constexpr int ENT_RIGHT = 0x04, ENT_FWD = 0x14, ENT_UP = 0x24, ENT_POS = 0x34;
constexpr int ENT_STATUS = 0x50;
constexpr int ENT_FLAGS_51 = 0x51;   // bit 0 bUsesCollision
constexpr int ENT_MODEL = 0x5C;    // short model index (read before ms_modelInfoPtrs lookups)
constexpr int PHYS_FLAGS1 = 0x52;  // bit 0x02 explosion proof
constexpr int PHYS_FLAGS2 = 0x53;  // 0x02 bullet, 0x04 fire, 0x08 collision, 0x10 melee
constexpr int PHYS_MOVE = 0x70, PHYS_TURN = 0x7C;
constexpr int PHYS_NUM_COLLISIONS = 0xE6, PHYS_COLLISIONS = 0xE8, PHYS_MAX_COLLISIONS = 6;   // what it just hit
constexpr int PHYS_MASS = 0xB8, PHYS_TURN_MASS = 0xBC;   // CAutomobile ctor copies them from handling
constexpr float RHINO_MASS = 18000.0f;                    // data\handling.cfg, RHINO
constexpr int PED_HEALTH = 0x354, PED_ARMOUR = 0x358;
constexpr int PED_STATE = 0x244, PED_DIE = 54, PED_DEAD = 55;   // 50 = PED_DRIVING
// CPed::InflictDamage(damagedBy, weaponType, damage, pedPiece, direction)
inline auto InflictDamage = (bool(__thiscall*)(void*, void*, int, float, int, unsigned char))0x525B20;
constexpr int PED_IN_VEHICLE = 0x3AC;   // (vehicle pointer: PED_MY_VEHICLE)
constexpr int PED_WEAPONS = 0x408, WEAPON_SIZE = 0x18, WEAPON_SLOTS = 10;
constexpr int WEAP_TYPE = 0x0, WEAP_STATE = 0x4, WEAP_CLIP = 0x8, WEAP_TOTAL = 0xC, WEAP_TIMER = 0x10;
constexpr int WEAPSTATE_RELOADING = 2;
constexpr int WEAPON_SNIPER = 28, WEAPON_LASER_SNIPER = 29, WEAPON_ROCKET_LAUNCHER = 30;
// CWeapon::FireProjectile(shooter, source, power) - what Fire runs for the rocket launcher (0x5D4885)
inline auto FireProjectile = (bool(__thiscall*)(void* weapon, void* shooter, float* source, float power))0x5CCF90;
constexpr int OBJECT_POOL_STRIDE = 0x1A0, MODEL_MISSILE = 273;
// CPed::SetCurrentWeapon(slot): weapon model out of / into the hand, slot recorded (player +0x60C too)
inline auto SetCurrentWeapon = (void(__thiscall*)(void* ped, int slot))0x4FF900;
// CWeapon::Fire(shooter, fireSource or null) - the game's own shot: ammo, bullet, effects
inline auto WeaponFire = (bool(__thiscall*)(void* weapon, void* shooter, float* source))0x5D45E0;
// CWeapon::FireInstantHit(shooter, source): the ordinary bullet (tracer, impact, blood, damage);
// for the player it follows the third-person crosshair. Fire sends the player's sniper
// shots to FireSniper (scope camera) instead (0x5D4786).
inline auto FireInstantHit = (bool(__thiscall*)(void* weapon, void* shooter, float* source))0x5D1140;
// cDMAudio::PlayOneShot(audioEntity, sound, volume) on DMAudio - Fire's own gunshot (0x5D4A70)
inline void* const DMAudio = (void*)0xA10B8A;
inline auto PlayOneShot = (void(__thiscall*)(void*, int, unsigned short, float))0x5F9DA0;
constexpr int ENT_AUDIO_ID = 0x64, SOUND_WEAPON_SHOT_FIRED = 0x37;
inline float* const ShotVolume = (float*)0x69D398;   // timer: next shot allowed
constexpr int PED_CUR_WEAPON = 0x504;   // signed char slot (GetWeapon, e.g. 0x429A4B)
constexpr int WEAPON_DETONATOR = 34;   // weapon.dat row 34 ("Detonator", model 291)
constexpr int PED_WANTED = 0x5F4, WANTED_LEVEL = 0x20;
constexpr int WANTED_COPS = 0x1E8;   // CCopPed* m_pCops[10]: cops chasing the player
// CWanted::ResetPolicePursuit: every chasing cop stops and goes back to wandering
inline auto ResetPolicePursuit = (void(__thiscall*)(void*))0x4D1D20;
inline auto PedSetWanderPath   = (void(__thiscall*)(void*, int))0x4FACC0;   // direction 0-7
constexpr int COP_IN_PURSUIT = 0x5FC;   // CCopPed::m_bIsInPursuit
constexpr int VEH_COLOR1 = 0x1A0, VEH_COLOR2 = 0x1A1;  // opcode 0229
constexpr int VEH_HEALTH = 0x204, VEH_DOORLOCK = 0x230, VEH_TYPE = 0x29C, VEH_DAMAGE = 0x2A0;
constexpr int MI_VEHTYPE = 0x3C;    // CVehicleModelInfo: 0 car, 1 boat, 5 bike
// CWeaponInfo (stride 0x64): model +0x54 and model2 +0x58 as read by
// CPed::GiveWeapon / CWeapon::Shutdown (0x5D4C90), slot +0x60. -1 = none.
constexpr int WI_RANGE = 0x04, WI_FIRE_RATE = 0x08, WI_DAMAGE = 0x14;   // CWeapon::Fire 0x5D4B7B..
constexpr int WI_FIRETYPE = 0x00, WI_CLIP = 0x10, WI_MODEL = 0x54, WI_MODEL2 = 0x58;
constexpr int MODEL_COUNT = 0x1964;   // RequestModel's own bound check

// ---- hook sites ------------------------------------------------------------
constexpr DWORD HOOK_PROCESS_SITE = 0x4A5DA0, GAME_PROCESS = 0x4A4410; // main loop -> CGame::Process
constexpr DWORD HOOK_PADS_SITE    = 0x4A4412, UPDATE_PADS  = 0x4AB6C0; // CGame::Process -> CPad::UpdatePads

template <class T> inline T& at(void* base, int off) { return *(T*)((char*)base + off); }

// CEntity virtuals (CWorld::Add calls vtable[0], CWorld::Remove calls vtable[1]):
// they put the entity into / take it out of the world sector lists.
// CVehicle::BlowUpCar(culprit) is vtable +0x80 (the BIGBANG cheat calls it with 0).
inline void VehicleBlowUp(void* v, void* culprit) { ((void(__thiscall*)(void*, void*))(*(void***)v)[0x80 / 4])(v, culprit); }
inline void EntityAdd(void* e)    { ((void(__thiscall*)(void*))(*(void***)e)[0])(e); }
inline void EntityRemove(void* e) { ((void(__thiscall*)(void*))(*(void***)e)[1])(e); }

} // namespace game
