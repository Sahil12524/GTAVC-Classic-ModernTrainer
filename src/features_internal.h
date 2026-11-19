// Shared by the feature files (settings, player, vehicle, combat, movement,
// teleport, peds, world). These were one features.cpp; what one part uses
// from another is declared here, everything else stays static in its file.
#pragma once
#include <unordered_set>
#include <unordered_map>
#include "features.h"
#include "trainer.h"
#include "game.h"
#include "tables.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

using namespace game;

// ---- settings.cpp: state, files, keys
extern DWORD g_killUntil;      // suicide in progress: protections are suspended until then
extern DWORD g_blastSafeUntil; // just put out of an exploding vehicle
FILE* OpenGameFile(const char* name, const char* mode, bool logMissing = true);
void PoolUse(void* poolPtr, int* used, int* size);
bool KeyHeld(int vk);
bool GameFocused();

// ---- player.cpp
extern bool g_onWater;         // standing on water this frame (WaterWalkTick)
extern bool g_overWater;       // walk on water: in the air just above the water (no falls here)
void KillTick(void* ped);
void ShowPosTick(void* me);
void PlayerExtrasTick(const Settings& A, void* me);

// ---- vehicle.cpp
extern int g_lastSpawned;
std::string Trim(std::string s);
bool IsLiveVehicle(void* p);
bool IsCopModel(int m);
void* SpawnVehicleObj(int model, bool warp);
bool VehicleWrecked(void* veh);
bool VehicleDamaged(void* veh);
void FixVehicle(void* veh);
void UnlockOnExit();
void DoorLockRelock();
void AutoLockTick(const Settings& A);
void HeavyCarTick(const Settings& A);
void AutoFlipTick(const Settings& A);
void StayOnBikeTick(const Settings& A);
void BikeExitFix(const Settings& A);
void FastExitTick(const Settings& A);
void PendingWarpTick();
void WreckTick(const Settings& A);
void CopCarEntryTick();
void VehicleExtrasTick(const Settings& A, void* me);
void CarGodFlagTick(const Settings& A, void* veh);

// ---- combat.cpp
extern bool g_flyingOnFoot;    // fly mode on, on foot (set by FlyTick)
extern bool g_bhopping;        // a bunny-hop chain is running (BhopTick)
// top / bottom: the box's ends from the bones at the tick, relative to the ped's position
struct EspEntry { short slot; unsigned char id; bool visible, locked; float dist; float top[3], bottom[3]; };
extern std::vector<EspEntry> g_espList;
bool IsLivePed(void* p);
bool IsHostile(void* p, void* me);
bool IsScopeWeapon(int t);
bool ScopedNow();
bool FirstPersonWeaponCam();
bool FireAnyView(char* w, void* me);
void BlowUpVehicle(void* v, void* me);
void DeleteCarWithPeople(void* v, void* me);
void TouchKillTick(const Settings& A);
void AimLockTick(const Settings& A, void* me);
void NoScopeSniperTick(const Settings& A, void* me);
void SniperAutoTick(const Settings& A, void* me);
void ShotTick(const Settings& A, void* me);
void DriveByAimTick(const Settings& A, void* me);
void RapidFireTick(const Settings& A, void* me);
void GenocideTick(const Settings& A, void* me);
void HeliAimTick(const Settings& A, void* me);
void EnemyScanTick(const Settings& A, void* me);
void ShotFxTick(const Settings& A, void* me);
void SuperPunchTick(const Settings& A, void* me);

// ---- movement.cpp
void SuperMoveTick(const Settings& A, void* me);
void WaterWalkTick(const Settings& A, void* me);
void BhopTick(const Settings& A, void* me);
void FlyTick();
void PushProjectilesOut(void* ped);

// ---- teleport.cpp
struct Footprint { float halfLen, halfWid, radius, bottom, top; bool avoidWater = true, openSky = false; };   // bottom/top: below/above the centre
Footprint EntityFootprint(void* e, bool vehicle);
void* BlipEntity(int index, int type);

// ---- peds.cpp
int PedH(void* p);
int CarH(void* v);
void* PedFromH(int h);
bool LoadModelNow(int m);
bool LoadWeaponModels(int type);
bool PedDying(void* p);
bool IsGuard(void* p);
void BodyguardTick(void* me);
void SpawnedPedsTick();

// ---- world.cpp
void ExtrasTick(const Settings& A, void* me);
void FovTick(void* me);

// ---- one mission-script command from the trainer --------------------------------
// CRunningScript::ProcessOneCommand (0x44FBE0) runs the command at the script's
// instruction pointer: +0x10, an offset into CTheScripts::ScriptSpace. One
// command is built here (opcode, then typed parameters: 1 int32, 3 local
// variable, 6 float, 8-byte text), a blank script is pointed at it and run -
// the very code a mission line goes through. Outputs land in its locals (+0x30).
namespace {
struct Scm {
    unsigned char b[96]; int n = 0;
    explicit Scm(int op) { w8(op & 0xFF); w8(op >> 8); }
    void w8(int v) { b[n++] = (unsigned char)v; }
    void w32(const void* p) { memcpy(b + n, p, 4); n += 4; }
    Scm& I(int v) { w8(1); w32(&v); return *this; }
    Scm& F(float v) { w8(6); w32(&v); return *this; }
    Scm& Out(int local) { w8(3); w8(local & 0xFF); w8(local >> 8); return *this; }
    Scm& Text(const char* s) { char t[8] = {}; strncpy(t, s, 8); memcpy(b + n, t, 8); n += 8; return *this; }
    // Returns local 0 (+0x30): where Out(0) puts a command's result - 009A's
    // handler stores the new handle with "mov [ebp + edi*4 + 0x30], esi"
    // (0x4498E5, checked in gta-vc.exe). The locals start at -1, so a command
    // that writes no result - or a create that VCModernFix refuses on a full
    // pool (it stores -1 there) - reads as "none", never as handle 0, which
    // names whatever lives in ped slot 0.
    int Run() {
        static char script[0x100];
        memset(script, 0, sizeof script);
        strcpy(script + 8, "vctrain");
        *(int*)(script + 0x10) = (int)((char*)b - ScriptSpace);
        for (int i = 0; i < 16; i++) *(int*)(script + 0x30 + i * 4) = -1;
        ProcessOneCommand(script);
        return *(int*)(script + 0x30);
    }
};
}
