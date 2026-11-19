// Peds: script handles and model loading for the trainer's own script
// commands, bodyguards, spawned peds (and handing their bodies back to the
// game), and the "everyone within 40 m" actions.
#include "features_internal.h"

// script handles: pool index << 8 | the slot's id byte (CPool::GetIndex)
static int PoolHandle(void* poolPtr, void* e, int stride) {
    char* pool = (char*)poolPtr;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* fl = *(unsigned char**)(pool + POOL_FLAGS);
    int i = (int)((char*)e - objs) / stride;
    return (i << 8) | fl[i];
}
int PedH(void* p) { return PoolHandle(*PedPool, p, PED_POOL_STRIDE); }
int CarH(void* v) { return PoolHandle(*VehiclePool, v, VEHICLE_POOL_STRIDE); }
// CPool<CPed>::GetAt (0x451CB0) doesn't check the slot (sar eax, 8 then reads
// flags[slot]): -1 would read the byte before the flags array. Checked here.
void* PedFromH(int h) {
    char* pool = (char*)*PedPool;
    if (!pool || h < 0 || (h >> 8) >= *(int*)(pool + POOL_SIZE)) return nullptr;
    return PoolGetPed(pool, h);
}

bool LoadModelNow(int m) {
    if (m <= 0 || m >= MODEL_COUNT) return false;
    if (StreamInfo[m * 20] == 1) return true;
    RequestModel(m, 1);
    LoadAllRequested(false);
    if (StreamInfo[m * 20] == 1) return true;
    Log("Model %d didn't load (streaming state %d)", m, StreamInfo[m * 20]);
    return false;
}
bool LoadWeaponModels(int type) {
    char* info = GetWeaponInfo(type);
    if (!info) return false;
    bool ok = true;
    for (int m : { *(int*)(info + WI_MODEL), *(int*)(info + WI_MODEL2) })
        if (m > 0 && m < MODEL_COUNT && !LoadModelNow(m)) ok = false;
    return ok;
}
bool PedDying(void* p) { int st = at<int>(p, PED_STATE); return st == PED_DIE || st == PED_DEAD || at<float>(p, PED_HEALTH) <= 0.0f; }

// ---- peds: bodyguards, spawned peds, nearby peds ------------------------------------
static std::vector<int> g_guards, g_spawnedPeds;
static std::unordered_set<int> g_guardSet;           // the same handles, for IsGuard over the whole pool
static void SyncGuardSet() { g_guardSet.clear(); g_guardSet.insert(g_guards.begin(), g_guards.end()); }
int BodyguardCount() { return (int)g_guards.size(); }

enum { CREATE_OK, CREATE_NO_MODEL, CREATE_POOL_FULL };
// Creates a ped with script command 009A on the trainer's own script object
// ("vctrain", ped type pedType). With VCModernFix a full ped pool (8 slots kept
// for the game) gives handle -1 instead of the crash at 0x4496E7.
static void* CreatePedNear(int model, int pedType, float dist, float angle, int* why) {
    *why = CREATE_OK;
    void* me = PlayerPed(); if (!me) { *why = CREATE_POOL_FULL; return nullptr; }
    if (!LoadModelNow(model)) { *why = CREATE_NO_MODEL; return nullptr; }
    void* ref = PlayerVehicle() ? PlayerVehicle() : me;
    const float* mp = &at<float>(ref, ENT_POS);
    float x = mp[0] + cosf(angle) * dist, y = mp[1] + sinf(angle) * dist;
    bool f = false;
    float gz = FindGroundZ3D(x, y, mp[2] + 2.0f, &f);
    float z = f && fabsf(gz - mp[2]) < 6.0f ? gz + 1.0f : mp[2];
    int h = Scm(0x9A).I(pedType).I(model).F(x).F(y).F(z).Out(0).Run();
    void* p = PedFromH(h);
    if (!p) *why = CREATE_POOL_FULL;
    return p;
}
static void Arm(void* p, int weapon) {
    if (weapon > 0 && LoadWeaponModels(weapon)) Scm(0x1B2).I(PedH(p)).I(weapon).I(9999).Run();
}
// One log line for a ped the game didn't make, with how full the pool is.
static void LogRefusedPed(const char* what, int n, int of, int model) {
    int used, size;
    PoolUse(*PedPool, &used, &size);
    Log("Spawn refused: %s %d of %d (model %d) - ped pool full, %d of %d slots used", what, n, of, model, used, size);
}
static void ShowPoolFull(const char* what, int made, int asked) {
    int used, size;
    PoolUse(*PedPool, &used, &size);
    ShowNative("%d of %d %s made - ped pool full (%d of %d used)", made, asked, what, used, size);
}
void SpawnBodyguards(int model, int count, int weapon) {
    void* me = PlayerPed(); if (!me) return;
    int made = 0, refused = 0;
    for (int i = 0; i < count; i++) {
        int why;
        void* p = CreatePedNear(model, 21 /* SPECIAL: no grudges */, 2.5f, 6.2831853f * i / (count > 0 ? count : 1), &why);
        if (why == CREATE_NO_MODEL) {
            ShowNative("Could not load that ped model");
            Log("Spawn: bodyguard model %d didn't load - none made", model);
            break;                                    // every other one would fail the same way
        }
        if (!p) { refused++; LogRefusedPed("bodyguard", i + 1, count, model); continue; }   // the next may fit (a street ped removed)
        int h = PedH(p);
        Arm(p, weapon);
        Scm(0x2E2).I(h).I(90).Run();                  // SET_CHAR_ACCURACY
        Scm(0x1DE).I(h).I(PedH(me)).Run();            // SET_CHAR_AS_LEADER: follows you, rides with you
        g_guards.push_back(h);
        made++;
    }
    SyncGuardSet();
    if (refused) ShowPoolFull("bodyguards", made, count);
    else if (made) ShowNative("%d bodyguard%s at your side", made, made == 1 ? "" : "s");
}
void DismissBodyguards() {
    for (int h : g_guards)
        if (void* p = PedFromH(h)) { Scm(0x1E0).I(h).Run(); Scm(0x1C2).I(h).Run(); (void)p; }
    g_guards.clear();
    SyncGuardSet();
    ShowNative("Bodyguards dismissed");
}
bool IsGuard(void* p) { return !g_guardSet.empty() && IsLivePed(p) && g_guardSet.count(PedH(p)) != 0; }
// Every half second each bodyguard goes for whoever is after you - aiming at
// you, attacking you, or the police while you're wanted - and comes back to
// your side when nobody is.
void BodyguardTick(void* me) {
    static DWORD next = 0;
    DWORD now = GetTickCount();
    if (now < next || g_guards.empty()) return;
    next = now + 500;
    std::vector<int> alive;
    for (int h : g_guards) { void* p = PedFromH(h); if (p && !PedDying(p)) alive.push_back(h); else if (p) Scm(0x1C2).I(h).Run(); }
    g_guards.swap(alive);
    SyncGuardSet();
    if (g_guards.empty()) return;
    const float* mp = &at<float>(me, ENT_POS);
    std::vector<void*> threats;
    if (char* pool = (char*)*PedPool) {
        char* objs = *(char**)(pool + POOL_OBJECTS);
        unsigned char* fl = *(unsigned char**)(pool + POOL_FLAGS);
        int size = *(int*)(pool + POOL_SIZE);
        for (int i = 0; i < size; i++) {
            if (fl[i] & 0x80) continue;
            void* p = objs + i * PED_POOL_STRIDE;
            if (p == me || PedDying(p) || IsGuard(p)) continue;
            const float* pp = &at<float>(p, ENT_POS);
            float d = sqrtf((pp[0] - mp[0]) * (pp[0] - mp[0]) + (pp[1] - mp[1]) * (pp[1] - mp[1]));
            if (d > 45.0f) continue;
            int obj = at<int>(p, PED_OBJECTIVE), st = at<int>(p, PED_STATE);
            bool afterMe = (obj == OBJ_KILL_CHAR_ON_FOOT || obj == OBJ_KILL_CHAR_ANY_MEANS) && at<void*>(p, PED_PED_IN_OBJECTIVE) == me;
            bool fighting = (st == PED_ATTACK || st == PED_FIGHT || st == PED_AIM_GUN) && IsHostile(p, me);
            bool cop = IsCopModel(at<short>(p, ENT_MODEL)) && GetWanted() > 0;
            if (afterMe || fighting || cop) threats.push_back(p);
        }
    }
    for (size_t i = 0; i < g_guards.size(); i++) {
        int h = g_guards[i];
        void* g = PedFromH(h);
        int obj = at<int>(g, PED_OBJECTIVE);
        void* cur = at<void*>(g, PED_PED_IN_OBJECTIVE);
        bool busy = (obj == OBJ_KILL_CHAR_ON_FOOT || obj == OBJ_KILL_CHAR_ANY_MEANS) && cur && IsLivePed(cur) && !PedDying(cur);
        if (!threats.empty()) {
            if (!busy) Scm(0x1CB).I(h).I(PedH(threats[i % threats.size()])).Run();   // SET_CHAR_OBJ_KILL_CHAR_ANY_MEANS
        } else if (!busy && obj != OBJ_FOLLOW_CHAR_IN_FORMATION && !at<unsigned char>(g, PED_IN_VEHICLE)) {
            Scm(0x1DE).I(h).I(PedH(me)).Run();        // back to your side
        }
    }
}
void SpawnPed(int model, bool hostile, int weapon) {
    void* me = PlayerPed(); if (!me) return;
    int why;
    void* p = CreatePedNear(model, hostile ? 18 /* CRIMINAL */ : 21, 6.0f, (rand() % 628) / 100.0f, &why);
    if (why == CREATE_NO_MODEL) { ShowNative("Could not load that ped model"); Log("Spawn: ped model %d didn't load", model); return; }
    if (!p) { LogRefusedPed("ped", 1, 1, model); ShowPoolFull("peds", 0, 1); return; }
    int h = PedH(p);
    Arm(p, weapon);
    if (hostile) Scm(0x1CB).I(h).I(PedH(me)).Run();   // comes after you
    g_spawnedPeds.push_back(h);
}

// The peds the trainer spawned, once dead, are handed back to the game the way
// VCModernFix's population.cpp does it and as the trainer already did for a
// dead bodyguard: MARK_CHAR_AS_NO_LONGER_NEEDED (01C2, which runs
// CTheScripts::CleanUpThisPed 0x45EB10 - its one-parameter handler at 0x45274D
// looks the handle up and calls it), so the game's own population code clears
// the body like any street body. Only handles in the trainer's own list are
// touched; a slot reused by someone else fails the id check in PedFromH.
void SpawnedPedsTick() {
    static DWORD next = 0;
    DWORD now = GetTickCount();
    if (now < next || g_spawnedPeds.empty()) return;
    next = now + 500;
    static int logs = 0;
    std::vector<int> keep;
    int released = 0;
    for (int h : g_spawnedPeds) {
        void* p = PedFromH(h);
        if (!p) continue;                                  // already gone
        if (PedDying(p)) { Scm(0x1C2).I(h).Run(); released++; continue; }
        keep.push_back(h);
    }
    g_spawnedPeds.swap(keep);
    if (released && logs++ < 20) Log("Peds: %d dead spawned ped%s handed back to the game (01C2)", released, released == 1 ? "" : "s");
}
void RemoveSpawnedPeds() {
    int n = 0;
    for (int h : g_spawnedPeds) if (void* p = PedFromH(h)) {
        if (at<unsigned char>(p, PED_IN_VEHICLE)) Scm(0x1C2).I(h).Run();
        else { Scm(0x9B).I(h).Run(); n++; }
    }
    g_spawnedPeds.clear();
    ShowNative("Removed %d spawned peds", n);
}
static std::vector<void*> BlipEntities() {
    std::vector<void*> out;
    for (auto& b : ActiveBlips()) if (b.type == 1 || b.type == 2) if (void* e = BlipEntity(b.index, b.type)) out.push_back(e);
    return out;
}
void NearbyPeds(int action) {
    void* me = PlayerPed(); if (!me) return;
    void* myVeh = PlayerVehicle();
    char* pool = (char*)*PedPool; if (!pool) return;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* fl = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    const float* mp = &at<float>(me, ENT_POS);
    auto marked = BlipEntities();
    std::vector<void*> list;
    for (int i = 0; i < size; i++) {
        if (fl[i] & 0x80) continue;
        void* p = objs + i * PED_POOL_STRIDE;
        if (p == me || PedDying(p) || IsGuard(p)) continue;
        if (myVeh && at<void*>(p, PED_MY_VEHICLE) == myVeh && at<unsigned char>(p, PED_IN_VEHICLE)) continue;
        const float* pp = &at<float>(p, ENT_POS);
        float d = sqrtf((pp[0] - mp[0]) * (pp[0] - mp[0]) + (pp[1] - mp[1]) * (pp[1] - mp[1]) + (pp[2] - mp[2]) * (pp[2] - mp[2]));
        if (d > 40.0f) continue;
        bool mission = false;
        for (void* e : marked) if (e == p || (at<unsigned char>(p, PED_IN_VEHICLE) && e == at<void*>(p, PED_MY_VEHICLE))) mission = true;
        if (action == NEARBY_DELETE && (mission || at<unsigned char>(p, PED_IN_VEHICLE))) continue;   // missions' people stay
        list.push_back(p);
    }
    for (size_t i = 0; i < list.size(); i++) {
        int h = PedH(list[i]);
        switch (action) {
        case NEARBY_FIRE:     Scm(0x326).I(h).Out(0).Run(); break;
        case NEARBY_FREEZE:   Scm(0x4D7).I(h).I(1).Run(); break;
        case NEARBY_UNFREEZE: Scm(0x4D7).I(h).I(0).Run(); break;
        case NEARBY_RIOT:
            if (list.size() > 1) Scm(0x1CB).I(h).I(PedH(list[(i + 1 + rand() % (list.size() - 1)) % list.size()])).Run();
            break;
        case NEARBY_DELETE:   Scm(0x9B).I(h).Run(); break;
        case NEARBY_RECRUIT:  if (!at<unsigned char>(list[i], PED_IN_VEHICLE)) Scm(0x1DE).I(h).I(PedH(me)).Run(); break;
        }
    }
    static const char* what[] = { "set on fire", "frozen", "unfrozen", "turned on each other", "deleted", "now follow you" };
    ShowNative("%d people nearby %s", (int)list.size(), what[action]);
    if (action == NEARBY_RECRUIT) {
        // Recruiting makes nobody, so there is no pool refusal; who followed is logged.
        int used, size;
        PoolUse(*PedPool, &used, &size);
        Log("Peds: %d people nearby told to follow you (ped pool %d of %d used)", (int)list.size(), used, size);
    }
}
