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
using DmgEngine_t = void (__thiscall*)(void* dm, int status);
using DmgWheel_t  = void (__thiscall*)(void* dm, int wheel, int status);
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
inline auto DmgSetEngine       = (DmgEngine_t) 0x5A97F0; // CDamageManager
inline auto DmgSetWheel        = (DmgWheel_t)  0x5A9840;
inline auto ForceWeatherNow    = (Weather_t)   0x57D550; // CWeather::ForceWeatherNow
inline auto SetGameClock       = (SetClock_t)  0x487160; // CClock::SetGameClock
inline auto LoadScene          = (LoadScene_t) 0x40AF60; // CStreaming::LoadScene
inline auto FindGroundZ        = (GroundZ_t)   0x4D5540; // CWorld::FindGroundZForCoord
inline auto AddToPCCheatString = (CheatChar_t) 0x4ABD20; // CPad::AddToPCCheatString

using PoolGet_t   = void* (__thiscall*)(void* pool, int handle);
using SetObj_t    = void (__thiscall*)(void* ped, int objective, void* target);
using Warp_t      = void (__thiscall*)(void* ped, void* vehicle);
inline auto PoolGetVehicle     = (PoolGet_t)   0x451C70; // CPool<CVehicle>::GetAt
inline auto PoolGetPed         = (PoolGet_t)   0x451CB0; // CPool<CPed>::GetAt
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
inline short* const ForcedWeather = (short*)0xA10A42;       // -1 = not forced
inline float* const TimeScale     = (float*)0x97F264;       // CTimer::ms_fTimeScale
inline float* const PedDensity    = (float*)0x694DC0;
inline float* const CarDensity    = (float*)0x686FC8;
inline char*  const ScriptSpace   = (char*)0x821280;
inline unsigned* const OnscnTimerOffset = (unsigned*)0x9B6CF8; // CUserDisplay::OnscnTimer entry 0
inline void** const ModelInfoPtrs = (void**)0x92D4C8;       // CModelInfo::ms_modelInfoPtrs
inline unsigned char* const StreamInfo = (unsigned char*)0x94DDD8; // ms_aInfoForModel, stride 20
inline unsigned char* const NeverTired = (unsigned char*)0x94AE68; // Players[0]+0x140 (opcode 0330)
inline unsigned char* const FastReload = (unsigned char*)0x94AE69; // Players[0]+0x141 (opcode 0331)
inline void** const VehiclePool = (void**)0xA0FDE4;         // CPools::ms_pVehiclePool
inline void** const PedPool     = (void**)0x97F2AC;         // CPools::ms_pPedPool
inline char* const RadarTrace   = (char*)0x7D7D38;          // CRadar::ms_RadarTrace[75]
// TheCamera's matrix: right +0, look direction +0x10, up +0x20, position +0x30
// (heading code passes 0x7E4698/0x7E469C to GetATanOfXY; pos 0x7E46B8 used as camera x,y).
inline float* const CameraFront = (float*)0x7E4698;
inline void* const TheCamera = (void*)0x7E4688;
// CCamera::SetCameraDirectlyBehindForFollowPed_CamOnAString (script opcode 0373)
inline auto CameraBehindPlayer = (void(__thiscall*)(void*))0x46BADE;
// CPad::UpdateMouse copies the raw mouse (MouseState, 0x7DD860) into this
// "current" state, which is what the camera and weapons actually read.
inline unsigned char* const NewMouseState = (unsigned char*)0x94D788;

// CBike::KnockOffRider(weaponType, direction, ped, getBackOn) - thiscall on the
// bike, ret 0x10. Called on crashes, hits, falls; 14 call sites.
constexpr DWORD KNOCK_OFF_RIDER = 0x613920;
constexpr int KNOCK_OFF_PROLOGUE = 10; // push ebx/esi/edi/ebp + sub esp,0xD0
constexpr int RADAR_COUNT = 75, RADAR_SIZE = 0x38;
constexpr int BLIP_TYPE = 0x04, BLIP_HANDLE = 0x08, BLIP_POS = 0x18, BLIP_INUSE = 0x27, BLIP_SPRITE = 0x34;
constexpr int POOL_OBJECTS = 0x0, POOL_FLAGS = 0x4, POOL_SIZE = 0x8, VEHICLE_POOL_STRIDE = 0x5DC;

// ---- object offsets ----------------------------------------------------------
constexpr int ENT_RIGHT = 0x04, ENT_FWD = 0x14, ENT_UP = 0x24, ENT_POS = 0x34;
constexpr int ENT_STATUS = 0x50;
constexpr int PHYS_FLAGS1 = 0x52;  // bit 0x02 explosion proof
constexpr int PHYS_FLAGS2 = 0x53;  // 0x02 bullet, 0x04 fire, 0x08 collision, 0x10 melee
constexpr int PHYS_MOVE = 0x70, PHYS_TURN = 0x7C;
constexpr int PHYS_MASS = 0xB8, PHYS_TURN_MASS = 0xBC;   // CAutomobile ctor copies them from handling
constexpr float RHINO_MASS = 18000.0f;                    // data\handling.cfg, RHINO
constexpr int PED_HEALTH = 0x354, PED_ARMOUR = 0x358;
constexpr int PED_VEHICLE = 0x3A8, PED_IN_VEHICLE = 0x3AC;
constexpr int PED_WEAPONS = 0x408, WEAPON_SIZE = 0x18, WEAPON_SLOTS = 10;
constexpr int WEAP_TYPE = 0x0, WEAP_CLIP = 0x8, WEAP_TOTAL = 0xC;
constexpr int WEAPON_DETONATOR = 34;   // weapon.dat row 34 ("Detonator", model 291)
constexpr int PED_WANTED = 0x5F4, WANTED_LEVEL = 0x20;
constexpr int VEH_COLOR1 = 0x1A0, VEH_COLOR2 = 0x1A1;  // opcode 0229
constexpr int VEH_HEALTH = 0x204, VEH_DOORLOCK = 0x230, VEH_TYPE = 0x29C, VEH_DAMAGE = 0x2A0;
constexpr int MI_VEHTYPE = 0x3C;    // CVehicleModelInfo: 0 car, 1 boat, 5 bike
// CWeaponInfo (stride 0x64): model +0x54 and model2 +0x58 as read by
// CPed::GiveWeapon / CWeapon::Shutdown (0x5D4C90), slot +0x60. -1 = none.
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
