// The player: health, armour, wanted level, money, weapons given, suicide,
// outfits and models, invisible / freeze position / forcefield, and the
// CS:GO-style speed meter.
#include "features_internal.h"

// ------------------------------------------------------------------ player
// The chosen max is remembered in the settings and put back every frame the
// game's value is lower (new game, loaded save, restart of the game).
void SetMaxHealth(int v) { v = v < 1 ? 1 : v > 255 ? 255 : v; *MaxHealth = (unsigned char)v; S.maxHealth = v; }
void SetMaxArmour(int v) { v = v < 1 ? 1 : v > 255 ? 255 : v; *MaxArmour = (unsigned char)v; S.maxArmour = v; }

// Suicide that always works: for a few seconds god mode, infinite armour,
// proofs and never-fall-off-bike are suspended (see Active), armour is
// removed and the game's own damage kills the player - on foot or in any
// vehicle. KillTick keeps health at 0 until the game has the player dying.
void KillPlayer() { g_killUntil = GetTickCount() + 15000; }

void KillTick(void* ped) {
    static bool died = false, hit = false;
    if (!g_killUntil) return;
    int state = at<int>(ped, PED_STATE);
    bool dying = state == PED_DIE || state == PED_DEAD;
    if (GetTickCount() >= g_killUntil || (died && !dying)) {   // respawned (or gave up)
        g_killUntil = 0; died = hit = false; return;
    }
    if (dying) { died = true; return; }
    at<float>(ped, PED_ARMOUR) = 0.0f;
    if (!hit) { InflictDamage(ped, nullptr, 45 /* UNIDENTIFIED */, 1000.0f, 0, 0); hit = true; }
    at<float>(ped, PED_HEALTH) = 0.0f;
}
void SetHealth(float v) { if (void* p = PlayerPed()) at<float>(p, PED_HEALTH) = v; }
void SetArmour(float v) { if (void* p = PlayerPed()) at<float>(p, PED_ARMOUR) = v; }

int GetWanted() {
    void* p = PlayerPed(); if (!p) return 0;
    void* w = at<void*>(p, PED_WANTED);
    return w ? at<int>(w, WANTED_LEVEL) : 0;
}
static DWORD g_noWantedUntil = 0;   // after Clear: keep stars off a moment

// The wanted hook (hooks.cpp) keeps the player at zero stars while this is
// true: never wanted is on, or Clear was pressed in the last 3 s (so a crime
// or script raising it again in the same moment doesn't bring it straight back).
bool g_onWater = false;   // standing on water this frame (WaterWalkTick)
bool g_overWater = false; // walk on water: in the air just above the water (no falls here)
bool NoRagdollOn() { return Active().noRagdoll || g_onWater || g_overWater; }

int WantedCap() {
    const Settings& A = Active();
    if (!A.wantedCapOn) return -1;
    return A.wantedCap < 0 ? 0 : A.wantedCap > 6 ? 6 : A.wantedCap;
}

bool HoldNoWanted() {
    return Active().neverWanted || (g_noWantedUntil && GetTickCount() < g_noWantedUntil);
}

int g_wantedAboveLimit = 0;

// Nested lifts (SetWantedLevel runs UpdateWantedLevel, which is hooked) share
// the outermost one's saved values, so savedLevel is always the game's real limit.
static int s_liftDepth = 0, s_realLevel = 0, s_realChaos = 0;
WantedLimitLift::WantedLimitLift(bool lift) : on(lift) {
    if (!on) return;
    if (s_liftDepth++ == 0) {
        s_realLevel = *MaxWantedLevel; s_realChaos = *MaxChaosLevel;
        if (*MaxWantedLevel < 6) *MaxWantedLevel = 6;
        if (*MaxChaosLevel < 9600) *MaxChaosLevel = 9600;   // 6 stars needs 4800+
    }
    savedLevel = s_realLevel; savedChaos = s_realChaos;
}
WantedLimitLift::~WantedLimitLift() {
    if (on && --s_liftDepth == 0) { *MaxWantedLevel = s_realLevel; *MaxChaosLevel = s_realChaos; }
}

void SetWanted(int level) {
    void* p = PlayerPed(); if (!p) return;
    if (level < 0) level = 0;
    if (level > 6) level = 6;
    if (level <= 0) g_noWantedUntil = GetTickCount() + 3000;
    else g_noWantedUntil = 0;                       // raising it on purpose
    // Above the story's limit (4 stars until late in the game): the game would
    // clamp it straight back, so lift the limit for this player only.
    g_wantedAboveLimit = level > *MaxWantedLevel ? level : 0;
    if (void* w = at<void*>(p, PED_WANTED)) {
        WantedLimitLift lift(g_wantedAboveLimit != 0);
        game::SetWantedLevel(w, level);
    }
}

static int ClampMoney(long long v) { return v < 0 ? 0 : v > 999999999 ? 999999999 : (int)v; }
void AddMoney(int amount) { *Money = ClampMoney((long long)*Money + amount); }
void SetMoney(int amount) { *Money = ClampMoney(amount); }

void GiveWeapon(int type, int ammo) {
    void* ped = PlayerPed(); if (!ped) return;
    char* info = GetWeaponInfo(type);
    int models[2] = { *(int*)(info + WI_MODEL), *(int*)(info + WI_MODEL2) };
    for (int m : models) if (m > 0 && m < MODEL_COUNT) RequestModel(m, 1);
    LoadAllRequested(false);
    // Never hand over a weapon whose model isn't in memory: the ped builds the
    // weapon object from it straight away and that would crash.
    for (int m : models) {
        if (m > 0 && m < MODEL_COUNT && StreamInfo[m * 20] != 1) {   // 1 = LOADED
            ShowNative("Weapon model not loaded, try again");
            Log("Weapon %d not given: its model %d didn't load (streaming state %d)", type, m, StreamInfo[m * 20]);
            return;
        }
    }
    game::GiveWeapon(ped, type, ammo, true);
}

// ---- player: outfit / model ------------------------------------------------------
const std::vector<PedModel>& PedModels() {
    static std::vector<PedModel> list;
    if (!list.empty()) return list;
    // Only models that have a file in the archive: default.ide also lists
    // empty slots (special01..special21) that crash the game if worn.
    std::unordered_set<std::string> files;
    if (FILE* dir = OpenGameFile("models\\gta3.dir", "rb")) {
        unsigned char e[32];
        while (fread(e, 1, 32, dir) == 32) {
            char nm[25] = {}; memcpy(nm, e + 8, 24);
            std::string s = nm; for (auto& c : s) c = (char)tolower((unsigned char)c);
            files.insert(s);
        }
        fclose(dir);
    }
    FILE* f = OpenGameFile("data\\default.ide", "r");
    if (!f) return list;
    char line[512]; bool in = false;
    while (fgets(line, sizeof line, f)) {
        std::string l = Trim(line);
        if (l == "peds") { in = true; continue; }
        if (!in) continue;
        if (l == "end") break;
        if (l.empty() || l[0] == '#') continue;
        // id, model, txd, ped type, stats, anim group, cars mask (hex), anim file, radio1, radio2
        std::vector<std::string> fl;
        for (size_t a = 0;;) {
            size_t b = l.find(',', a);
            fl.push_back(Trim(l.substr(a, b == std::string::npos ? std::string::npos : b - a)));
            if (b == std::string::npos) break;
            a = b + 1;
        }
        if (fl.size() < 2) continue;
        int id = atoi(fl[0].c_str());
        std::string name = fl[1];
        std::string low = name; for (auto& c : low) c = (char)tolower((unsigned char)c);
        if (!files.empty() && !files.count(low + ".dff")) continue;
        if (id <= 0 || name == "null") continue;
        PedModel m; m.id = id; m.name = name;
        if (fl.size() > 2) m.txd = fl[2];
        if (fl.size() > 3) m.type = fl[3];
        if (fl.size() > 4) m.stat = fl[4];
        if (fl.size() > 5) m.anim = fl[5];
        if (fl.size() > 6) m.cars = (unsigned)strtoul(fl[6].c_str(), nullptr, 16);
        if (fl.size() > 8) m.radio1 = atoi(fl[8].c_str());
        if (fl.size() > 9) m.radio2 = atoi(fl[9].c_str());
        list.push_back(m);
    }
    fclose(f);
    return list;
}

const PedStat* PedStatOf(const std::string& statName) {
    static std::unordered_map<std::string, PedStat> stats;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        if (FILE* f = OpenGameFile("data\\pedstats.dat", "r")) {
            char line[256];
            while (fgets(line, sizeof line, f)) {
                char nm[64]; PedStat s;
                if (line[0] == '#') continue;
                if (sscanf(line, "%63s %f %f %d %d %d %d %f %f %d", nm, &s.flee, &s.heading, &s.fear, &s.temper,
                           &s.law, &s.sexy, &s.attack, &s.defend, &s.flags) == 10)
                    stats[nm] = s;
            }
            fclose(f);
        }
    }
    auto it = stats.find(statName);
    return it == stats.end() ? nullptr : &it->second;
}

// Changing what the player wears, the way CPed::Undress / CPed::Dress do it,
// but loading the model ourselves so a model that fails to load never gets
// put on (that crashed the game in DRESS_CHAR, and a second UNDRESS on a ped
// with no model left crashed in CPed::Undress).
//  - Tommy's outfits (player, player2..9) have no model ID of their own: the
//    clothes shops load them into model 0 by name (RequestSpecialModel).
//  - Any other ped model is loaded by its own ID. Loading it into model 0 by
//    name fails when that ped is already loaded in the world (busy places).
namespace {

using AssocCb_t    = void  (__cdecl*)(void* assoc, void* ped);
using PedFn_t      = void  (__cdecl*)(void* ped);
using SpecialReq_t = void  (__cdecl*)(int model, const char* name, int flags);
using Dress_t      = void  (__thiscall*)(void* ped);
using DelRw_t      = void  (__thiscall*)(void* ent);
auto AssocFinishCb      = (AssocCb_t)   0x4F5AD0; // what Undress runs on anim 0xA5
auto PedDropWeaponModel = (PedFn_t)     0x4DB310; // Undress: hand weapon off
auto RequestSpecial     = (SpecialReq_t)0x40AA60; // CStreaming::RequestSpecialModel
auto PedDress           = (Dress_t)     0x4EEFD0; // CPed::Dress: rebuild from +0x5C
}
static bool ModelReady(int mi) {
    return mi >= 0 && mi < MODEL_COUNT && ModelInfoPtrs[mi] &&
           StreamInfo[mi * 20] == 1 && at<void*>(ModelInfoPtrs[mi], 0x28);   // loaded and has its clump
}
// Take the player's current model off (CPed::Undress without the request).
static void UndressPed(void* me) {
    if (void* clump = at<void*>(me, 0x4C)) {
        if (void* a = GetAnimAssoc(clump, 0xA5)) AssocFinishCb(a, me);
        ((DelRw_t)(*(void***)me)[6])(me);                 // DeleteRwObject
    }
    PedDropWeaponModel(me);
}
static void DressPed(void* me, int mi) { at<short>(me, ENT_MODEL) = (short)mi; PedDress(me); }

std::string CurrentPlayerSkin() {
    void* me = PlayerPed(); if (!me) return "PLAYER";
    int mi = at<short>(me, ENT_MODEL);
    if (mi > 0) {
        for (auto& e : PedModels()) if (e.id == mi) return e.name;
        return "PLAYER";
    }
    // model 0 carries the outfit's name (the clothes shops rename it)
    const char* n = ModelInfoPtrs[0] ? (const char*)ModelInfoPtrs[0] + 4 : nullptr;
    char buf[25] = {};
    if (n) memcpy(buf, n, 24);
    if (_strnicmp(buf, "player", 6) != 0) return "PLAYER";
    return buf;
}

bool SetPlayerSkin(const char* name, bool quiet) {
    void* me = PlayerPed(); if (!me || !name || !*name) return false;
    if (PlayerVehicle()) { ShowNative("Get out of the vehicle to change clothes"); return false; }
    if (!at<void*>(me, 0x4C)) return false;               // mid-change already: leave it to the game
    char low[25] = {}; strncpy(low, name, 24);
    for (char* c = low; *c; c++) *c = (char)tolower((unsigned char)*c);

    int id = -1;
    for (auto& e : PedModels()) if (!_stricmp(e.name.c_str(), low)) { id = e.id; break; }
    if (id > 0) {                                         // an ordinary ped model
        if (!LoadModelNow(id) || !ModelReady(id)) { ShowNative("Couldn't load \"%s\"", name); return false; }
        UndressPed(me);
        DressPed(me, id);
        if (!quiet) ShowNative("Now wearing: %s", name);
        return true;
    }
    // one of Tommy's outfits: into model 0 by name, as the clothes shops do
    if (strncmp(low, "player", 6) != 0) { ShowNative("Can't wear \"%s\": no such model", name); return false; }
    UndressPed(me);
    RequestSpecial(0, low, 6);
    LoadAllRequested(false);
    bool ok = ModelReady(0);
    if (!ok) { RequestSpecial(0, "player", 6); LoadAllRequested(false); }
    if (!ModelReady(0)) { RequestModel(0, 6); LoadAllRequested(false); }
    if (!ModelReady(0)) { ShowNative("Couldn't load Tommy's model"); return false; }   // never dress into nothing
    DressPed(me, 0);
    if (ok) { if (!quiet) ShowNative("Now wearing: %s", name); }
    else    ShowNative("Can't wear \"%s\": that model isn't in the game files", name);
    return true;
}

void RemoveAllWeapons() {
    void* me = PlayerPed(); if (!me) return;
    Scm(0x48F).I(PedH(me)).Run();
    ShowNative("All weapons removed");
}

// ---- player: invisible / freeze / forcefield ---------------------------------------
// Wake a sleeping car / object so the physics moves it this frame. A car
// driving "on rails" (traffic: no physics, moved along the road) is first
// switched to real physics, exactly as an explosion does.
static void WakePhysical(void* e) {
    unsigned char& st = at<unsigned char>(e, ENT_STATUS);
    if ((st & 7) == 2 && (st >> 3) == 2) {                       // a vehicle, STATUS_SIMPLE
        st = (st & 7) | (3 << 3);                                // STATUS_PHYSICS
        SwitchVehicleToRealPhysics(e);
    }
    at<unsigned char>(e, ENT_FLAGS_51) &= ~ENT_STATIC_BIT;
    if (!at<void*>(e, PHYS_MOVING_NODE)) AddToMovingList(e);   // the game doesn't check: never add twice
}
void PlayerExtrasTick(const Settings& A, void* me) {
    // invisible: the ped (and, with the car option, the vehicle) isn't drawn
    static bool hidMe = false;
    unsigned char& vis = at<unsigned char>(me, ENT_FLAGS_52);
    if (A.invisible) { vis &= ~ENT_VISIBLE_BIT; hidMe = true; }
    else if (hidMe) { vis |= ENT_VISIBLE_BIT; hidMe = false; }

    static void* hidCar = nullptr;
    void* veh = PlayerVehicle();
    if (hidCar && (hidCar != veh || !A.invisibleCar)) {
        if (IsLiveVehicle(hidCar)) at<unsigned char>(hidCar, ENT_FLAGS_52) |= ENT_VISIBLE_BIT;
        hidCar = nullptr;
    }
    if (A.invisibleCar && veh) { at<unsigned char>(veh, ENT_FLAGS_52) &= ~ENT_VISIBLE_BIT; hidCar = veh; }

    // freeze position: the vehicle you're in (FREEZE_CAR_POSITION 0519), or you
    // on foot (FREEZE_CHAR_POSITION 04D7) - even in mid air
    static void* frozen = nullptr; static bool frozenCar = false;
    void* want = A.freezePos ? (veh ? veh : me) : nullptr;
    if (frozen != want) {
        if (frozen && (frozenCar ? IsLiveVehicle(frozen) : IsLivePed(frozen))) {
            if (frozenCar) Scm(0x519).I(CarH(frozen)).I(0).Run(); else Scm(0x4D7).I(PedH(frozen)).I(0).Run();
        }
        frozen = want; frozenCar = want && want == veh;
        if (frozen) { if (frozenCar) Scm(0x519).I(CarH(frozen)).I(1).Run(); else Scm(0x4D7).I(PedH(frozen)).I(1).Run(); }
    }

    // forcefield, every frame. Push: people are knocked over and thrown back,
    // cars are thrown away. Destroy: cars blow up and people die. Rockets and
    // grenades are pushed off either way. Your vehicle, the people in it and
    // your own people (bodyguards, followers) are never touched.
    if (A.forcefield) {
        bool destroy = A.forcefieldMode == 1;
        const float* mp = &at<float>(veh ? veh : me, ENT_POS);
        // Menyoo's radius (set_local_forcefield): 10 m around you for the push,
        // straight-line distance, the same for people, vehicles and objects;
        // its explode mode blasts 5 m around you.
        const float R = destroy ? 5.0f : 10.0f;
        const float rPed = R, rCar = R;
        if (char* pool = (char*)*PedPool) {
            char* objs = *(char**)(pool + POOL_OBJECTS);
            unsigned char* fl = *(unsigned char**)(pool + POOL_FLAGS);
            int size = *(int*)(pool + POOL_SIZE);
            for (int i = 0; i < size; i++) {
                if (fl[i] & 0x80) continue;
                void* p = objs + i * PED_POOL_STRIDE;
                if (p == me || PedDying(p) || IsGuard(p)) continue;
                if (at<int>(p, PED_OBJECTIVE) == OBJ_FOLLOW_CHAR_IN_FORMATION) continue;
                if (at<unsigned char>(p, PED_IN_VEHICLE)) continue;               // their car gets it instead
                float* pp = &at<float>(p, ENT_POS);
                float dx = pp[0] - mp[0], dy = pp[1] - mp[1], dz = pp[2] - mp[2];
                float d = sqrtf(dx * dx + dy * dy);
                if (sqrtf(d * d + dz * dz) > rPed) continue;
                if (d < 0.1f) { dx = 1; dy = 0; d = 1; }
                if (destroy) {
                    // Collision forced on only for a ped on foot, and only for this one call:
                    // never for one opening a door / jacking / getting in or out (states 50-53,
                    // 56-60), which then collides with its own car (freeze at 0x42335C).
                    const int st = at<int>(p, PED_STATE);
                    const bool boarding = (st >= 50 && st <= 53) || (st >= 56 && st <= 60);
                    if (!InflictDamage(p, me, 45, 1000.0f, 0, 0) && !boarding) {
                        const unsigned char was = at<unsigned char>(p, 0x51);
                        at<unsigned char>(p, 0x51) = was | 1;
                        InflictDamage(p, me, 45, 1000.0f, 0, 0);
                        at<unsigned char>(p, 0x51) = (at<unsigned char>(p, 0x51) & ~1) | (was & 1);
                    }
                    at<float>(p, PED_ARMOUR) = 0.0f; at<float>(p, PED_HEALTH) = 0.0f;
                    continue;
                }
                // the closer, the harder the shove (0.6 at the edge, 2.0 right against you)
                float k = 1.0f - d / rPed;
                float push = 0.6f + 1.4f * k * k;
                float* mv = &at<float>(p, PHYS_MOVE);
                if (mv[0] * dx / d + mv[1] * dy / d >= push * 0.9f) continue;     // already flying off fast enough
                if (at<int>(p, PED_STATE) != PED_FALL) PedSetFall(p, 2000, 0x1B, true);
                // Not standing any more (as the game does when a blast throws
                // someone): the ped's own walking no longer cancels the throw.
                at<unsigned char>(p, PED_FLAGS_14C) &= ~0x01;
                mv[0] = dx / d * push; mv[1] = dy / d * push; mv[2] = 0.3f + 0.2f * k;
            }
        }
        if (char* pool = (char*)*VehiclePool) {
            char* objs = *(char**)(pool + POOL_OBJECTS);
            unsigned char* fl = *(unsigned char**)(pool + POOL_FLAGS);
            int size = *(int*)(pool + POOL_SIZE);
            int blasts = 0;
            // how far each car was last frame: one that isn't getting further
            // away (its own wheels / brakes or the game's car physics ate the
            // shove) is moved outward directly
            static std::unordered_map<void*, float> lastDist;
            std::unordered_map<void*, float> nowDist;
            for (int i = 0; i < size; i++) {
                if (fl[i] & 0x80) continue;
                void* v = objs + i * VEHICLE_POOL_STRIDE;
                if (v == veh || (destroy && VehicleWrecked(v))) continue;         // wrecks get pushed too
                const float* vp = &at<float>(v, ENT_POS);
                float dx = vp[0] - mp[0], dy = vp[1] - mp[1], dz = vp[2] - mp[2];
                float d = sqrtf(dx * dx + dy * dy);
                if (sqrtf(d * d + dz * dz) > rCar) continue;
                if (d < 0.1f) { dx = 1; dy = 0; d = 1; }
                if (destroy) {
                    if (blasts < 2) { BlowUpVehicle(v, me); blasts++; }           // a couple a frame: no stall
                    continue;
                }
                // The closer, the harder: 0.8 at the edge up to 3.0 right up against you.
                // Before, anything touching was already "moving away" from the last
                // shove (the collision pushed it back) and got skipped.
                float k = 1.0f - d / rCar;
                float push = 0.8f + 2.2f * k * k;
                float* mv = &at<float>(v, PHYS_MOVE);
                nowDist[v] = d;
                const float ux = dx / d, uy = dy / d;                             // away from you
                // Cars are held by their wheels (brakes, handbrake, tyre grip):
                // a speed alone gets eaten by the road. So, like a blast does,
                // the car is lifted clear of the road first, then thrown - and
                // while it's inside the field it's carried outward every frame
                // too, so a heavy or braked car can't stay put.
                WakePhysical(v);
                const float shell = 4.5f;                                         // no car ever closer than this
                float step = fminf(push * fminf(*TimeStep, 3.0f) * 0.5f, 0.6f);
                if (d < shell) step = fmaxf(step, shell - d);
                bool f = false;
                float ground = FindGroundZ3D(vp[0], vp[1], vp[2] + 1.0f, &f);
                // tyres just clear of the road: up to ~0.5 m over its usual ride
                // height (centre ~1 m up), never more however high the frame rate
                float raise = f ? fminf(0.25f, (ground + 1.5f) - vp[2]) : 0.0f;
                EntityRemove(v);
                float* pos = &at<float>(v, ENT_POS);
                pos[0] += ux * step; pos[1] += uy * step;
                if (raise > 0.0f) pos[2] += raise;
                MatrixUpdateRW((char*)v + 4);                                     // the drawn car moves with it
                EntityAdd(v);
                nowDist[v] = d + step;
                // the throw itself (kept up every frame, so it leaves the field flying)
                mv[0] = ux * push; mv[1] = uy * push;
                float lift = 0.15f + 0.25f * k;
                if (mv[2] < lift) mv[2] = lift;
                memset(&at<float>(v, PHYS_TURN), 0, 12);                          // no spinning in place against you
            }
            lastDist.swap(nowDist);
        }
        // loose objects too (bins, boxes, cones - fixed ones like lampposts ignore it)
        if (!destroy) if (char* pool = (char*)*ObjectPool) {
            char* objs = *(char**)(pool + POOL_OBJECTS);
            unsigned char* fl = *(unsigned char**)(pool + POOL_FLAGS);
            int size = *(int*)(pool + POOL_SIZE);
            for (int i = 0; i < size; i++) {
                if (fl[i] & 0x80) continue;
                void* o = objs + i * OBJECT_POOL_STRIDE;
                if (at<short>(o, ENT_MODEL) == MODEL_MISSILE) continue;            // rockets: PushProjectilesOut
                const float* op = &at<float>(o, ENT_POS);
                float dx = op[0] - mp[0], dy = op[1] - mp[1], dz = op[2] - mp[2];
                float d = sqrtf(dx * dx + dy * dy);
                if (sqrtf(d * d + dz * dz) > rPed) continue;
                if (d < 0.1f) { dx = 1; dy = 0; d = 1; }
                float k = 1.0f - d / rPed, push = 0.5f + 1.5f * k * k;
                float mass = at<float>(o, PHYS_MASS);
                if (!(mass > 0.0f && mass < 5000.0f)) continue;                    // immovable props stay put
                float* mv = &at<float>(o, PHYS_MOVE);
                if (mv[0] * dx / d + mv[1] * dy / d >= push * 0.9f) continue;
                WakePhysical(o);
                mv[0] = dx / d * push; mv[1] = dy / d * push; if (mv[2] < 0.2f) mv[2] = 0.2f;
            }
        }
        PushProjectilesOut(me);
    }
}

// ---- speed meter (CS:GO cl_showpos) ----------------------------------------------
// Position, view angles and speed of the player (or the vehicle they're in).
// The speed comes from how far it really moved each frame, so the trainer's
// own movement (bunny hop, surf, fly mode, super run) counts too.
static ShowPos g_showPos{};
static bool g_showPosOk = false;
void ShowPosTick(void* me) {
    static float last[3];
    static bool have = false;
    static LARGE_INTEGER freq, prev;
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    void* e = PlayerVehicle() ? PlayerVehicle() : me;
    g_showPosOk = e != nullptr;
    if (!e) { have = false; return; }
    const float* p = &at<float>(e, ENT_POS);
    float dt = prev.QuadPart ? (float)(now.QuadPart - prev.QuadPart) / (float)freq.QuadPart : 0.0f;
    prev = now;
    float speed = g_showPos.vel;                                        // m/s, horizontal (as CS:GO speed HUDs)
    if (have && dt > 0.0005f && dt < 0.25f) {
        float dx = p[0] - last[0], dy = p[1] - last[1];
        float v = sqrtf(dx * dx + dy * dy) / dt;
        if (v < 400.0f) speed = speed * 0.7f + v * 0.3f;                 // jumps (teleports) don't count
        if (v < 0.05f && speed < 0.05f) speed = 0.0f;
    }
    memcpy(last, p, 12); have = true;
    g_showPos.vel = speed;
    memcpy(g_showPos.pos, p, 12);
    const float* f = CameraFront;
    g_showPos.ang[0] = -asinf(fmaxf(-1.0f, fminf(1.0f, f[2]))) * 57.29578f;   // pitch, + = looking down
    g_showPos.ang[1] = atan2f(f[1], f[0]) * 57.29578f;                         // yaw
    g_showPos.ang[2] = 0.0f;
}
bool GetShowPos(ShowPos& out) {
    if (!g_showPosOk || *GameMenuActive) return false;
    out = g_showPos;
    out.vel *= S.showPosUnits == 1 ? 3.6f : S.showPosUnits == 2 ? 1.0f : 39.37f;   // u/s: 1 unit = 1 inch
    return true;
}
