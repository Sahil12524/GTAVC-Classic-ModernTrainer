#include "features.h"
#include "trainer.h"
#include "game.h"
#include "tables.h"
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

using namespace game;

Settings S;
static std::vector<std::function<void()>> g_queue;

const Settings& Active() {
    static Settings off = [] { Settings s; s.enabled = false; s.warpIntoSpawned = false; return s; }();
    if (S.enabled) return S;
    off.flySpeed = S.flySpeed;
    return off;
}

void SetTrainerEnabled(bool on) {
    S.enabled = on;
    ShowNative("VC Trainer %s", on ? "ON" : "OFF - all cheats paused");
}

void Queue(std::function<void()> fn) { g_queue.push_back(std::move(fn)); }

// ------------------------------------------------------------------ helpers
static wchar_t g_msg[256];
void ShowNative(const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    int i = 0;
    for (; buf[i] && i < 255; i++) g_msg[i] = (unsigned char)buf[i];
    g_msg[i] = 0;
    game::SetHelpMessage(g_msg, true, false);
}

void* PlayerPed() { return FindPlayerPed(); }
void* PlayerVehicle() { return FindPlayerPed() ? FindPlayerVehicle() : nullptr; }

bool GetPlayerPos(float out[3]) {
    void* e = PlayerVehicle();
    if (!e) e = PlayerPed();
    if (!e) return false;
    memcpy(out, &at<float>(e, ENT_POS), 12);
    return true;
}

static void ApplyProofs(void* ent, const Proofs& p, Proofs& last, bool force) {
    // Only touch bits whose checkbox is on, or that were just switched off,
    // so proofs set by missions are left alone.
    auto bit = [&](int off, unsigned char mask, bool now, bool before) {
        unsigned char& b = at<unsigned char>(ent, off);
        if (now) b |= mask;
        else if (before || force) b &= ~mask;
    };
    bit(PHYS_FLAGS2, 0x02, p.bullet, last.bullet);
    bit(PHYS_FLAGS2, 0x04, p.fire, last.fire);
    bit(PHYS_FLAGS1, 0x02, p.explosion, last.explosion);
    bit(PHYS_FLAGS2, 0x08, p.collision, last.collision);
    bit(PHYS_FLAGS2, 0x10, p.melee, last.melee);
    last = p;
}

// ------------------------------------------------------------------ player
void SetMaxHealth(int v) { *MaxHealth = (unsigned char)(v < 1 ? 1 : v > 255 ? 255 : v); }
void SetMaxArmour(int v) { *MaxArmour = (unsigned char)(v < 1 ? 1 : v > 255 ? 255 : v); }
void SetHealth(float v) { if (void* p = PlayerPed()) at<float>(p, PED_HEALTH) = v; }
void SetArmour(float v) { if (void* p = PlayerPed()) at<float>(p, PED_ARMOUR) = v; }

int GetWanted() {
    void* p = PlayerPed(); if (!p) return 0;
    void* w = at<void*>(p, PED_WANTED);
    return w ? at<int>(w, WANTED_LEVEL) : 0;
}
void SetWanted(int level) {
    void* p = PlayerPed(); if (!p) return;
    if (void* w = at<void*>(p, PED_WANTED)) game::SetWantedLevel(w, level);
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
            return;
        }
    }
    game::GiveWeapon(ped, type, ammo, true);
}

// ------------------------------------------------------------------ vehicles
static std::vector<VehicleEntry> g_vehicles;
static int g_lastSpawned = 141; // Infernus until something is spawned

static void WarpPlayerInto(void* veh) {
    void* ped = PlayerPed();
    if (!ped || !veh) return;
    PedSetObjective(ped, 0x12, veh);   // OBJECTIVE_ENTER_CAR_AS_DRIVER, as opcode 0369 does
    WarpPedIntoCar(ped, veh);
}

static std::string Trim(std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

static std::string GameText(const std::string& key) {
    const wchar_t* w = TextGet(TheText, key.c_str());
    std::string out;
    for (int i = 0; w && w[i] && i < 64; i++) out += (w[i] < 128) ? (char)w[i] : '?';
    return out;
}

const std::vector<VehicleEntry>& VehicleList() {
    if (!g_vehicles.empty()) return g_vehicles;
    FILE* f = fopen("data\\default.ide", "r");
    if (!f) return g_vehicles;
    char line[512]; bool inCars = false;
    while (fgets(line, sizeof line, f)) {
        std::string l = Trim(line);
        if (l == "cars") { inCars = true; continue; }
        if (!inCars) continue;
        if (l == "end") break;
        if (l.empty() || l[0] == '#') continue;
        std::vector<std::string> col; size_t s = 0, e;
        while ((e = l.find(',', s)) != std::string::npos) { col.push_back(Trim(l.substr(s, e - s))); s = e + 1; }
        col.push_back(Trim(l.substr(s)));
        if (col.size() < 6) continue;
        int id = atoi(col[0].c_str());
        const std::string& cls = col[3];
        int cat;
        if (cls == "bike") cat = CAT_BIKES;
        else if (cls == "boat") cat = CAT_BOATS;
        else if (cls == "car") {
            switch (id) {
            case 155: case 177: case 199: case 217: case 218: case 227: cat = CAT_HELIS; break;
            case 171: case 194: case 195: case 231: cat = CAT_RC; break;
            default: cat = CAT_CARS;
            }
        } else continue; // trains, planes on rails, police chopper: not driveable
        if (id <= 0 || id >= 6500 || !ModelInfoPtrs[id]) continue;
        std::string name = GameText(col[5]);
        if (name.empty() || name.size() < 2) name = col[1];
        g_vehicles.push_back({ id, cat, name + "  (" + col[1] + ")" });
    }
    fclose(f);
    return g_vehicles;
}

void SpawnVehicle(int model) {
    void* ped = PlayerPed(); if (!ped || !ModelInfoPtrs[model]) return;
    int type = at<int>(ModelInfoPtrs[model], MI_VEHTYPE);

    unsigned char* si = StreamInfo + model * 20;
    bool keep = (si[1] & 1) != 0;
    RequestModel(model, 1);
    LoadAllRequested(false);
    if (si[0] != 1) { ShowNative("Could not load that vehicle model"); return; }
    if (!keep) { SetModelDeletable(model); SetModelTxdDeletable(model); }

    void* veh = nullptr;
    if (type == 5)      { if (void* m = VehicleNew(0x4EC)) veh = BikeCtor(m, model, 1); }
    else if (type == 1) { if (void* m = VehicleNew(0x4C0)) veh = BoatCtor(m, model, 1); }
    else                { if (void* m = VehicleNew(0x5DC)) veh = AutomobileCtor(m, model, 1); }
    if (!veh) { ShowNative("Vehicle pool is full"); return; }

    void* ref = PlayerVehicle(); if (!ref) ref = ped;
    float* pos = &at<float>(ref, ENT_POS);
    float fx = at<float>(ref, ENT_FWD), fy = at<float>(ref, ENT_FWD + 4);
    float len = sqrtf(fx * fx + fy * fy);
    if (len < 0.01f) { fx = 0; fy = 1; } else { fx /= len; fy /= len; }
    float dist = (type == 5) ? 4.0f : 6.0f;

    float* m = &at<float>(veh, ENT_RIGHT);
    m[0] = fy;  m[1] = -fx; m[2] = 0;             // right
    float* f = &at<float>(veh, ENT_FWD);  f[0] = fx; f[1] = fy; f[2] = 0;
    float* u = &at<float>(veh, ENT_UP);   u[0] = 0;  u[1] = 0;  u[2] = 1;
    float* p = &at<float>(veh, ENT_POS);
    p[0] = pos[0] + fx * dist; p[1] = pos[1] + fy * dist; p[2] = pos[2] + 0.5f;

    unsigned char& st = at<unsigned char>(veh, ENT_STATUS);
    st = (st & 7) | 0x20;                 // STATUS_ABANDONED, like the game's spawn cheat
    at<int>(veh, VEH_DOORLOCK) = 1;       // unlocked
    WorldAdd(veh);
    g_lastSpawned = model;

    if (S.warpIntoSpawned && !PlayerVehicle()) WarpPlayerInto(veh);
}

int LastSpawnedModel() { return g_lastSpawned; }

void WarpIntoNearestVehicle() {
    void* ped = PlayerPed(); if (!ped || PlayerVehicle()) return;
    char* pool = (char*)*VehiclePool; if (!pool) return;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    float* pp = &at<float>(ped, ENT_POS);
    void* best = nullptr; float bestD = 60.0f * 60.0f;
    for (int i = 0; i < size; i++) {
        if (flags[i] & 0x80) continue;   // free slot
        void* v = objs + i * VEHICLE_POOL_STRIDE;
        float* vp = &at<float>(v, ENT_POS);
        float dx = vp[0] - pp[0], dy = vp[1] - pp[1], dz = vp[2] - pp[2];
        float d = dx * dx + dy * dy + dz * dz;
        if (d < bestD && at<float>(v, VEH_HEALTH) > 250.0f) { bestD = d; best = v; }
    }
    if (best) WarpPlayerInto(best);
    else ShowNative("No vehicle within 60m");
}

void SetVehicleColours(int c1, int c2) {
    if (void* v = PlayerVehicle()) { at<unsigned char>(v, VEH_COLOR1) = (unsigned char)c1; at<unsigned char>(v, VEH_COLOR2) = (unsigned char)c2; }
}
bool GetVehicleColours(int& c1, int& c2) {
    void* v = PlayerVehicle(); if (!v) return false;
    c1 = at<unsigned char>(v, VEH_COLOR1); c2 = at<unsigned char>(v, VEH_COLOR2);
    return true;
}

const std::vector<PaletteColour>& CarPalette() {
    static std::vector<PaletteColour> pal;
    static bool loaded = false;
    if (loaded) return pal;
    loaded = true;
    FILE* f = fopen("data\\carcols.dat", "r");
    if (!f) return pal;
    char line[256]; bool inCol = false;
    while (fgets(line, sizeof line, f)) {
        std::string l = Trim(line);
        if (l == "col") { inCol = true; continue; }
        if (!inCol) continue;
        if (l == "end") break;
        int r, g, b;
        if (sscanf(l.c_str(), "%d , %d , %d", &r, &g, &b) != 3) continue;
        std::string name;
        size_t hash = l.find('#');
        if (hash != std::string::npos) {   // "# 3 cherry red   red" -> "cherry red"
            std::string c = Trim(l.substr(hash + 1));
            size_t sp = c.find_first_of(" \t");
            if (sp != std::string::npos) c = Trim(c.substr(sp));
            size_t gap = c.find('\t');
            name = Trim(gap != std::string::npos ? c.substr(0, gap) : c);
        }
        pal.push_back({ (unsigned char)r, (unsigned char)g, (unsigned char)b, name });
    }
    fclose(f);
    return pal;
}

void JumpVehicle(float amount) {
    if (void* v = PlayerVehicle()) at<float>(v, PHYS_MOVE + 8) += amount;
}

float VehicleSpeedKmh() {
    void* v = PlayerVehicle(); if (!v) return 0;
    float* m = &at<float>(v, PHYS_MOVE);
    return sqrtf(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]) * 180.0f;
}

void RepairVehicle() {
    void* veh = PlayerVehicle(); if (!veh) return;
    at<float>(veh, VEH_HEALTH) = 1000.0f;
    if (at<int>(veh, VEH_TYPE) == 0) {    // automobile: engine + tyres, like ASPIRINE
        void* dm = (char*)veh + VEH_DAMAGE;
        DmgSetEngine(dm, 0);
        for (int w = 0; w < 4; w++) DmgSetWheel(dm, w, 0);
    }
}

void FlipVehicle() {
    void* veh = PlayerVehicle(); if (!veh) return;
    float* f = &at<float>(veh, ENT_FWD);
    float fx = f[0], fy = f[1], len = sqrtf(fx * fx + fy * fy);
    if (len < 0.01f) { fx = 0; fy = 1; } else { fx /= len; fy /= len; }
    float* r = &at<float>(veh, ENT_RIGHT); r[0] = fy; r[1] = -fx; r[2] = 0;
    f[0] = fx; f[1] = fy; f[2] = 0;
    float* u = &at<float>(veh, ENT_UP); u[0] = 0; u[1] = 0; u[2] = 1;
    at<float>(veh, ENT_POS + 8) += 1.5f;
    memset(&at<float>(veh, PHYS_MOVE), 0, 12);
    memset(&at<float>(veh, PHYS_TURN), 0, 12);
}

// The car the trainer locked. When the player gets out of it, its doors are
// unlocked again - a locked car can't be re-entered, not even by the player.
static void* g_trainerLocked = nullptr;

void SetDoorLock(bool locked) {
    void* v = PlayerVehicle();
    if (!v) { ShowNative("You are not in a vehicle"); return; }
    at<int>(v, VEH_DOORLOCK) = locked ? 2 : 1;
    g_trainerLocked = locked ? v : nullptr;
    ShowNative(locked ? "Doors locked" : "Doors unlocked");
}

// True if p is a vehicle that still exists in the game's vehicle pool.
static bool IsLiveVehicle(void* p) {
    char* pool = (char*)*VehiclePool;
    if (!pool || !p) return false;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    int off = (int)((char*)p - objs);
    if (off < 0 || off % VEHICLE_POOL_STRIDE) return false;
    int i = off / VEHICLE_POOL_STRIDE;
    return i < size && !(flags[i] & 0x80);
}

// ------------------------------------------------------------------ heavy car
// The player's car gets the Rhino's mass, so the game's own physics shoves
// other cars aside. Optionally, any car it touches is also flung away or blown up.
static void HeavyCarTick(const Settings& A) {
    static void* heavy = nullptr;        // car whose mass we changed
    static float origMass = 0, origTurnMass = 0;
    static std::vector<std::pair<void*, DWORD>> hitTimes;

    void* veh = PlayerVehicle();
    bool want = A.heavyCar && veh;

    if (heavy && (!want || heavy != veh)) {           // restore the car we changed
        if (IsLiveVehicle(heavy)) {
            at<float>(heavy, PHYS_MASS) = origMass;
            at<float>(heavy, PHYS_TURN_MASS) = origTurnMass;
        }
        heavy = nullptr;
    }
    if (!want) return;
    if (heavy != veh) {
        heavy = veh;
        origMass = at<float>(veh, PHYS_MASS);
        origTurnMass = at<float>(veh, PHYS_TURN_MASS);
    }
    if (origMass > 0 && origMass < RHINO_MASS) {
        at<float>(veh, PHYS_MASS) = RHINO_MASS;
        at<float>(veh, PHYS_TURN_MASS) = origTurnMass * (RHINO_MASS / origMass);
    }

    if (A.heavyMode == 0) return;
    float* mv = &at<float>(veh, PHYS_MOVE);
    float mySpeed = sqrtf(mv[0] * mv[0] + mv[1] * mv[1]);
    if (mySpeed < 0.03f) return;                      // ~5 km/h: parked next to a car does nothing

    char* pool = (char*)*VehiclePool; if (!pool) return;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    float* p = &at<float>(veh, ENT_POS);
    float* r = &at<float>(veh, ENT_RIGHT);
    float* f = &at<float>(veh, ENT_FWD);
    DWORD now = GetTickCount();

    for (int i = 0; i < size; i++) {
        if (flags[i] & 0x80) continue;
        void* o = objs + i * VEHICLE_POOL_STRIDE;
        if (o == veh || at<float>(o, VEH_HEALTH) <= 0.0f) continue;   // self, or already wrecked
        float* op = &at<float>(o, ENT_POS);
        float d[3] = { op[0] - p[0], op[1] - p[1], op[2] - p[2] };
        if (fabsf(d[2]) > 3.0f) continue;
        // in the player car's own frame: touching = within its box plus a car's width
        float side = d[0] * r[0] + d[1] * r[1], ahead = d[0] * f[0] + d[1] * f[1];
        if (fabsf(side) > 2.8f || fabsf(ahead) > 4.8f) continue;

        bool recent = false;
        for (auto& h : hitTimes) if (h.first == o && now - h.second < 1500) recent = true;
        if (recent) continue;
        hitTimes.push_back({ o, now });

        if (A.heavyMode == 2) {
            VehicleBlowUp(o, nullptr);
        } else {
            float len = sqrtf(d[0] * d[0] + d[1] * d[1]);
            float dx = len > 0.01f ? d[0] / len : f[0], dy = len > 0.01f ? d[1] / len : f[1];
            float push = fmaxf(0.8f, mySpeed * 2.5f);
            float* om = &at<float>(o, PHYS_MOVE);
            om[0] = dx * push; om[1] = dy * push; om[2] = 0.35f;
            float* ot = &at<float>(o, PHYS_TURN);
            ot[0] += 0.02f; ot[2] += (side > 0 ? 0.05f : -0.05f);
        }
    }
    if (hitTimes.size() > 64) {                        // forget old hits
        std::vector<std::pair<void*, DWORD>> keep;
        for (auto& h : hitTimes) if (now - h.second < 1500) keep.push_back(h);
        hitTimes.swap(keep);
    }
}

// ---- never fall off bike -------------------------------------------------
// The knock-off hook reports every time the game throws the player off a bike;
// the game finishes the fall normally and we put the player straight back on.
static void* g_kbBike = nullptr;
static DWORD g_kbTime = 0;
static float g_kbMove[3];
static bool  g_forcingOff = false;     // our own ForceOffBike: don't re-seat
static DWORD g_exitPressTime = 0;      // last time the exit button was pressed on a bike

void BikeKnockedOff(void* bike) {
    if (g_forcingOff || !bike) return;
    g_kbBike = bike;
    g_kbTime = GetTickCount();
    memcpy(g_kbMove, &at<float>(bike, PHYS_MOVE), 12);
}

static void ForceOff(void* bike, void* ped) {
    g_forcingOff = true;
    ForceOffBike(bike, ped);
    g_forcingOff = false;
    g_kbBike = nullptr;
}

static bool BikeUsable(void* bike) {
    return IsLiveVehicle(bike) && at<float>(bike, VEH_HEALTH) > 0.0f &&
           (at<unsigned char>(bike, ENT_STATUS) >> 3) != 5;          // not STATUS_WRECKED
}

static void StandBikeUp(void* bike) {
    float* f = &at<float>(bike, ENT_FWD);
    float len = sqrtf(f[0] * f[0] + f[1] * f[1]);
    float fx = len > 0.01f ? f[0] / len : 0.0f, fy = len > 0.01f ? f[1] / len : 1.0f;
    EntityRemove(bike);
    float* r = &at<float>(bike, ENT_RIGHT); r[0] = fy; r[1] = -fx; r[2] = 0;
    f[0] = fx; f[1] = fy; f[2] = 0;
    float* u = &at<float>(bike, ENT_UP); u[0] = 0; u[1] = 0; u[2] = 1;
    at<float>(bike, ENT_POS + 8) += 0.6f;
    memset(&at<float>(bike, PHYS_TURN), 0, 12);
    EntityAdd(bike);
}

static void StayOnBikeTick(const Settings& A) {
    void* bike = g_kbBike;
    if (!bike) return;
    DWORD now = GetTickCount();
    void* ped = PlayerPed();
    if (!A.stayOnBike || !ped || now - g_kbTime > 1500) { g_kbBike = nullptr; return; }
    if (now - g_exitPressTime < 2000) { g_kbBike = nullptr; return; } // player asked to get off
    void* cur = PlayerVehicle();
    if (cur && cur != bike) { g_kbBike = nullptr; return; }          // took another vehicle
    if (cur == bike && at<unsigned char>(ped, PED_IN_VEHICLE)) return; // fall not finished yet
    g_kbBike = nullptr;
    if (!BikeUsable(bike) || at<float>(ped, PED_HEALTH) <= 1.0f) return; // dead bike / dying: let go
    // Falls in quick succession (each within 1.5 s of the last) form a streak.
    static DWORD lastReseat = 0;
    static int streak = 0;
    streak = (now - lastReseat < 1500) ? streak + 1 : 1;
    lastReseat = now;
    if (streak > 10) {                       // e.g. sinking in water: nothing will hold
        ShowNative("Couldn't keep you on the bike");
        streak = 0;
        return;
    }

    bool tipped = at<float>(bike, ENT_UP + 8) < 0.6f;
    if (tipped) StandBikeUp(bike);
    float* mv = &at<float>(bike, PHYS_MOVE);
    float now3[3]; memcpy(now3, mv, 12);
    float before = sqrtf(g_kbMove[0] * g_kbMove[0] + g_kbMove[1] * g_kbMove[1] + g_kbMove[2] * g_kbMove[2]);
    float after  = sqrtf(now3[0] * now3[0] + now3[1] * now3[1] + now3[2] * now3[2]);
    // Lost a lot of speed = hit something (wall, car). Restoring the old speed
    // would just drive the bike back into it, so keep the speed it has now.
    bool hit = after < before * 0.6f;

    WarpPlayerInto(bike);
    if (streak >= 2 || tipped) {             // repeated crash: put the bike at rest
        memset(mv, 0, 12);
        memset(&at<float>(bike, PHYS_TURN), 0, 12);
    } else if (hit) {
        memcpy(mv, now3, 12);
    } else {
        memcpy(mv, g_kbMove, 12);            // shot / bump: keep riding at the same speed
    }
}

// Bike exit rules (Pads[0]: NewState.Triangle +0x1E is the exit button,
// +0xF0 DisablePlayerControls - checked so cutscenes/missions aren't broken):
//  - exit pressed and the game doesn't start getting off within 0.2 s (it
//    refuses while the bike is in the air): eject the player anyway;
//  - exit held 1.5 s while still on: eject (never fall off bike only);
//  - on the bike but not riding (broken state) for 3 s: eject (same).
static void BikeExitFix(const Settings& A) {
    static DWORD heldSince = 0, oddSince = 0, pressAt = 0;
    static bool prevTri = false;
    void* ped = PlayerPed();
    void* veh = PlayerVehicle();
    bool tri = at<short>(Pad0, 0x1E) != 0;
    bool pressed = tri && !prevTri;
    prevTri = tri;
    if (!A.enabled || g_menuOpen || !ped || !veh || at<int>(veh, VEH_TYPE) != 5 ||
        !at<unsigned char>(ped, PED_IN_VEHICLE)) { heldSince = oddSince = pressAt = 0; return; }
    bool controls = at<short>(Pad0, 0xF0) == 0;
    DWORD now = GetTickCount();
    bool riding = (at<void*>(veh, 0x1A8) == ped || at<void*>(veh, 0x1AC) == ped) &&   // driver / passenger
                  at<int>(ped, 0x244) == 0x32;                                        // PED_DRIVING

    if (pressed && controls) { pressAt = now; g_exitPressTime = now; }
    if (pressAt && now - pressAt > 200) {
        pressAt = 0;
        if (riding) { ForceOff(veh, ped); heldSince = oddSince = 0; return; }        // game refused
    }

    if (!A.stayOnBike) { heldSince = oddSince = 0; return; }
    if (riding) oddSince = 0;
    else if (!oddSince) oddSince = now;
    else if (now - oddSince > 3000) { ForceOff(veh, ped); oddSince = heldSince = 0; return; }

    if (!tri || !controls) { heldSince = 0; return; }
    if (!heldSince) heldSince = now;
    if (now - heldSince > 1500) { ForceOff(veh, ped); heldSince = 0; }
}

static void UnlockOnExit() {
    static void* lastVeh = nullptr;
    void* cur = PlayerVehicle();
    if (lastVeh && cur != lastVeh && lastVeh == g_trainerLocked) {
        if (IsLiveVehicle(lastVeh) && at<int>(lastVeh, VEH_DOORLOCK) == 2) {
            at<int>(lastVeh, VEH_DOORLOCK) = 1;
            ShowNative("Doors unlocked so you can get back in");
        }
        g_trainerLocked = nullptr;
    }
    lastVeh = cur;
}

void BoostVehicle(float amount) {
    void* veh = PlayerVehicle(); if (!veh) return;
    float* f = &at<float>(veh, ENT_FWD);
    float* mv = &at<float>(veh, PHYS_MOVE);
    for (int i = 0; i < 3; i++) mv[i] += f[i] * amount;
}

void StopVehicle() {
    void* veh = PlayerVehicle(); if (!veh) return;
    memset(&at<float>(veh, PHYS_MOVE), 0, 12);
    memset(&at<float>(veh, PHYS_TURN), 0, 12);
}

// ------------------------------------------------------------------ world
void SetTime(int h, int m) {
    SetGameClock((unsigned char)h, (unsigned char)m);
    S.frozenHour = h; S.frozenMinute = m;
}
void ForceWeather(int type) { ForceWeatherNow(type); }
void ReleaseWeather() { *ForcedWeather = -1; }

// ------------------------------------------------------------------ teleport
static std::vector<SavedPos> g_saved;
static bool g_savedLoaded = false;
static const char* kSaveFile = "VCTrainer_positions.txt";

std::vector<SavedPos>& SavedPositions() {
    if (!g_savedLoaded) {
        g_savedLoaded = true;
        if (FILE* f = fopen(kSaveFile, "r")) {
            char line[256];
            while (fgets(line, sizeof line, f)) {
                char name[128]; float x, y, z;
                if (sscanf(line, "%f %f %f %127[^\r\n]", &x, &y, &z, name) == 4)
                    g_saved.push_back({ name, x, y, z });
            }
            fclose(f);
        }
    }
    return g_saved;
}

void SavePositions() {
    if (FILE* f = fopen(kSaveFile, "w")) {
        for (auto& p : g_saved) fprintf(f, "%.3f %.3f %.3f %s\n", p.x, p.y, p.z, p.name.c_str());
        fclose(f);
    }
}

void TeleportTo(float x, float y, float z, bool findGround) {
    void* ped = PlayerPed(); if (!ped) return;
    float target[3] = { x, y, z };
    LoadScene(target);                     // stream the area (collision) in first
    if (findGround) target[2] = FindGroundZ(x, y) + 1.0f;
    void* ent = PlayerVehicle(); if (!ent) ent = ped;
    memcpy(&at<float>(ent, ENT_POS), target, 12);
    memset(&at<float>(ent, PHYS_MOVE), 0, 12);
}

std::vector<Blip> ActiveBlips() {
    std::vector<Blip> out;
    for (int i = 0; i < RADAR_COUNT; i++) {
        char* t = RadarTrace + i * RADAR_SIZE;
        if (!t[BLIP_INUSE]) continue;
        Blip b{ i, *(int*)(t + BLIP_TYPE), *(short*)(t + BLIP_SPRITE), {} };
        int handle = *(int*)(t + BLIP_HANDLE);
        void* ent = nullptr;
        switch (b.type) {
        case 1: if (*VehiclePool) ent = PoolGetVehicle(*VehiclePool, handle); break; // car
        case 2: if (*PedPool) ent = PoolGetPed(*PedPool, handle); break;             // character
        case 4: case 5: memcpy(b.pos, t + BLIP_POS, 12); break;                      // coord / contact point
        default: continue;                                                           // objects, pickups: skip
        }
        if (b.type == 1 || b.type == 2) {
            if (!ent) continue;
            memcpy(b.pos, &at<float>(ent, ENT_POS), 12);
        }
        out.push_back(b);
    }
    return out;
}

void TeleportToBlip(const Blip& b) {
    float x = b.pos[0], y = b.pos[1], z = b.pos[2];
    if (b.type == 1 || b.type == 2) { x += 3.0f; TeleportTo(x, y, z + 1.0f, false); return; }
    TeleportTo(x, y, z, z < -50.0f || z == 0.0f);
    if (z > -50.0f && z != 0.0f) {          // coord blip with a real height: stand on it
        void* ent = PlayerVehicle(); if (!ent) ent = PlayerPed();
        if (ent) at<float>(ent, ENT_POS + 8) = z + 1.0f;
    }
}

bool TeleportToMarker() {
    auto blips = ActiveBlips();
    const Blip* pick = nullptr;
    for (auto& b : blips) if (b.sprite == 0) { pick = &b; break; }  // mission markers have no icon
    if (!pick) { ShowNative("No mission marker on the radar"); return false; }
    TeleportToBlip(*pick);
    return true;
}

// ------------------------------------------------------------------ settings & hotkeys
int g_capturingHotkey = -1;

std::vector<HotkeyAction>& HotkeyActions() {
    static std::vector<HotkeyAction> list;
    if (!list.empty()) return list;
    using K = HotkeyAction::Kind;
    auto tog = [&](const char* cat, const char* id, const char* label, bool* v) {
        list.push_back({ id, label, cat, K::Toggle, v, nullptr });
    };
    auto press = [&](const char* cat, std::string id, std::string label, std::function<void()> fn) {
        list.push_back({ std::move(id), std::move(label), cat, K::Press, nullptr, std::move(fn) });
    };
    auto hold = [&](const char* cat, const char* id, const char* label, std::function<void()> fn) {
        list.push_back({ id, label, cat, K::Hold, nullptr, std::move(fn) });
    };

    // ids of the first hotkeys are kept so older VCTrainer.ini bindings still load
    press("General", "master", "Trainer on/off (all cheats)", [] { SetTrainerEnabled(!S.enabled); });
    press("General", "menu_camera_reset", "Camera behind player", [] { CameraBehindPlayer(TheCamera); });
    press("General", "all_off", "Turn every cheat off", [] { TurnEverythingOff(); });

    const char* P = "Player";
    tog(P, "god", "Infinite health", &S.godMode);
    tog(P, "infinite_armour", "Infinite armour", &S.infiniteArmour);
    tog(P, "never_wanted", "Never wanted", &S.neverWanted);
    tog(P, "infinite_ammo", "Infinite ammo", &S.infiniteAmmo);
    tog(P, "no_reload", "No reload", &S.noReload);
    tog(P, "never_tired", "Infinite sprint", &S.neverTired);
    tog(P, "fast_reload", "Fast reload", &S.fastReload);
    tog(P, "fly", "Fly mode", &S.flyMode);
    press(P, "fly_faster", "Fly speed +10", [] { S.flySpeed = fminf(S.flySpeed + 10, 150); ShowNative("Fly speed %.0f", S.flySpeed); });
    press(P, "fly_slower", "Fly speed -10", [] { S.flySpeed = fmaxf(S.flySpeed - 10, 5); ShowNative("Fly speed %.0f", S.flySpeed); });
    tog(P, "p_bullet", "Bulletproof", &S.playerProofs.bullet);
    tog(P, "p_fire", "Fireproof", &S.playerProofs.fire);
    tog(P, "p_explosion", "Explosion proof", &S.playerProofs.explosion);
    tog(P, "p_collision", "Collision proof", &S.playerProofs.collision);
    tog(P, "p_melee", "Melee proof", &S.playerProofs.melee);
    press(P, "fill_health", "Fill health", [] { SetHealth((float)*MaxHealth); });
    press(P, "fill_armour", "Fill armour", [] { SetArmour((float)*MaxArmour); });
    press(P, "clear_wanted", "Clear wanted level", [] { SetWanted(0); });
    press(P, "wanted_up", "Wanted level +1", [] { SetWanted(GetWanted() < 6 ? GetWanted() + 1 : 6); });
    press(P, "wanted_down", "Wanted level -1", [] { SetWanted(GetWanted() > 0 ? GetWanted() - 1 : 0); });

    const char* M = "Money";
    press(M, "money_1k", "+$1,000", [] { AddMoney(1000); });
    press(M, "money_10k", "+$10,000", [] { AddMoney(10000); });
    press(M, "money_100k", "+$100,000", [] { AddMoney(100000); });
    press(M, "money_1m", "+$1,000,000", [] { AddMoney(1000000); });
    press(M, "money_take", "-$10,000", [] { AddMoney(-10000); });

    const char* V = "Vehicle";
    tog(V, "stay_on_bike", "Never fall off bike", &S.stayOnBike);
    tog(V, "heavy_car", "Heavy car (like the Rhino)", &S.heavyCar);
    press(V, "heavy_mode", "Heavy car: change what touching does", [] {
        S.heavyMode = (S.heavyMode + 1) % 3;
        const char* m[] = { "just heavy", "push cars far away", "blow cars up" };
        ShowNative("Heavy car: %s", m[S.heavyMode]);
    });
    tog(V, "car_god", "Car god mode", &S.carGodMode);
    tog(V, "auto_repair", "Auto repair", &S.autoRepair);
    tog(V, "warp_into_spawned", "Put me in spawned vehicles", &S.warpIntoSpawned);
    tog(V, "c_bullet", "Car bulletproof", &S.carProofs.bullet);
    tog(V, "c_fire", "Car fireproof", &S.carProofs.fire);
    tog(V, "c_explosion", "Car explosion proof", &S.carProofs.explosion);
    tog(V, "c_collision", "Solid car (collision proof)", &S.carProofs.collision);
    tog(V, "c_melee", "Car melee proof", &S.carProofs.melee);
    press(V, "repair", "Repair vehicle", [] { RepairVehicle(); });
    press(V, "flip", "Flip vehicle upright", [] { FlipVehicle(); });
    press(V, "lock_doors", "Lock doors", [] { SetDoorLock(true); });
    press(V, "unlock_doors", "Unlock doors", [] { SetDoorLock(false); });
    hold(V, "boost", "Boost", [] { BoostVehicle(0.04f); });
    press(V, "jump", "Vehicle jump", [] { JumpVehicle(0.35f); });
    press(V, "stop", "Stop vehicle", [] { StopVehicle(); });
    press(V, "spawn_last", "Spawn last vehicle", [] { SpawnVehicle(g_lastSpawned); });
    press(V, "nearest_car", "Get into nearest vehicle", [] { WarpIntoNearestVehicle(); });
    press(V, "random_paint", "Random paint", [] {
        int n = (int)CarPalette().size();
        if (n) SetVehicleColours(rand() % n, rand() % n);
    });

    const char* W = "World";
    tog(W, "freeze_time", "Freeze time", &S.freezeTime);
    tog(W, "freeze_mission_timer", "Disable mission timers", &S.freezeMissionTimer);
    press(W, "hour_plus", "Time +1 hour", [] { SetTime((*ClockHours + 1) % 24, *ClockMinutes); });
    press(W, "weather_next", "Next weather", [] {
        static int w = 0; w = (w + 1) % 7; ForceWeather(w);
        const char* names[] = { "Sunny", "Cloudy", "Rain", "Fog", "Extra sunny", "Hurricane", "Extra colours" };
        ShowNative("Weather: %s", names[w]);
    });
    press(W, "weather_normal", "Normal weather", [] { ReleaseWeather(); });
    press(W, "slow_motion", "Slow motion on/off", [] { *TimeScale = (*TimeScale < 0.9f) ? 1.0f : 0.5f; });
    press(W, "speed_reset", "Normal game speed", [] { *TimeScale = 1.0f; });

    const char* T = "Teleport";
    press(T, "tp_marker", "Teleport to mission marker", [] { TeleportToMarker(); });
    press(T, "save_pos", "Save current position", [] {
        float p[3];
        if (!GetPlayerPos(p)) return;
        auto& l = SavedPositions();
        l.push_back({ "Location " + std::to_string(l.size() + 1), p[0], p[1], p[2] });
        SavePositions();
        ShowNative("Saved %s", l.back().name.c_str());
    });
    for (int i = 1; i <= 9; i++) {
        press(T, "tp_saved_" + std::to_string(i), "Teleport to saved location " + std::to_string(i), [i] {
            auto& l = SavedPositions();
            if ((int)l.size() < i) { ShowNative("No saved location %d", i); return; }
            TeleportTo(l[i - 1].x, l[i - 1].y, l[i - 1].z, false);
        });
    }

    const char* O = "Overlay";
    tog(O, "overlay_coords", "Show coordinates", &S.overlayCoords);
    tog(O, "overlay_speed", "Show vehicle speed", &S.overlaySpeed);
    tog(O, "overlay_fps", "Show FPS", &S.overlayFps);
    tog(O, "overlay_active", "Show active cheats list", &S.overlayActive);

    for (auto& g : kWeapons)
        for (auto& w : g.w) {
            if (!w.type) break;
            int type = w.type, ammo = w.ammo;
            press("Give weapon", std::string("weapon_") + std::to_string(type), w.name,
                  [type, ammo] { ::GiveWeapon(type, ammo); });
        }

    for (auto& g : kCheats)
        for (auto& c : g.c) {
            if (!c.code) break;
            std::string id = "cheat_" + std::string(c.code);
            for (auto& ch : id) ch = (char)tolower(ch);
            const char* code = c.code;
            if (NativeCheatHasState(code))   // switches: each press flips it on/off
                press("Game cheats", id, std::string(c.desc) + " (on/off)",
                      [code] { SetNativeCheat(code, !NativeCheatOn(code)); });
            else
                press("Game cheats", id, c.desc, [code] { RunNativeCheat(code); });
        }
    return list;
}

static bool KeyHeld(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

int CurrentModifiers() {
    return (KeyHeld(VK_CONTROL) ? HKMOD_CTRL : 0) | (KeyHeld(VK_SHIFT) ? HKMOD_SHIFT : 0) |
           (KeyHeld(VK_MENU) ? HKMOD_ALT : 0);
}

bool IsModifierKey(int vk) {
    switch (vk) {
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
    case VK_MENU: case VK_LMENU: case VK_RMENU: return true;
    }
    return false;
}

void BindCapturedKey(int vk, int mods) {
    if (g_capturingHotkey < 0) return;
    HotkeyAction& h = HotkeyActions()[g_capturingHotkey];
    h.vk = vk;
    h.mods = IsModifierKey(vk) ? 0 : mods;   // a lone modifier is bound by itself
    g_capturingHotkey = -1;
    SaveSettings();
}

static void KeyName(int vk, char* out, int size) {
    switch (vk) {
    case VK_LBUTTON: snprintf(out, size, "Left mouse"); return;
    case VK_RBUTTON: snprintf(out, size, "Right mouse"); return;
    case VK_MBUTTON: snprintf(out, size, "Middle mouse"); return;
    case VK_XBUTTON1: snprintf(out, size, "Mouse 4"); return;
    case VK_XBUTTON2: snprintf(out, size, "Mouse 5"); return;
    case VK_CONTROL: snprintf(out, size, "Ctrl"); return;
    case VK_SHIFT: snprintf(out, size, "Shift"); return;
    case VK_MENU: snprintf(out, size, "Alt"); return;
    }
    LONG lp = (LONG)(MapVirtualKeyA(vk, 0) << 16);
    switch (vk) {   // extended keys need bit 24 for the right name
    case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_DIVIDE: case VK_NUMLOCK:
        lp |= 1 << 24;
    }
    if (!GetKeyNameTextA(lp, out, size)) snprintf(out, size, "Key %d", vk);
}

void HotkeyName(int vk, int mods, char* out, int size) {
    if (!vk) { snprintf(out, size, "-"); return; }
    char key[48];
    KeyName(vk, key, sizeof key);
    snprintf(out, size, "%s%s%s%s", (mods & HKMOD_CTRL) ? "Ctrl + " : "",
             (mods & HKMOD_SHIFT) ? "Shift + " : "", (mods & HKMOD_ALT) ? "Alt + " : "", key);
}

static const char* kIni = "VCTrainer.ini";
static struct { const char* key; bool* val; } kBools[] = {
    {"enabled", &S.enabled}, {"stay_on_bike", &S.stayOnBike}, {"heavy_car", &S.heavyCar},
    {"god_mode", &S.godMode}, {"infinite_armour", &S.infiniteArmour}, {"never_wanted", &S.neverWanted},
    {"infinite_ammo", &S.infiniteAmmo}, {"no_reload", &S.noReload}, {"never_tired", &S.neverTired},
    {"fast_reload", &S.fastReload}, {"car_god", &S.carGodMode}, {"auto_repair", &S.autoRepair},
    {"warp_into_spawned", &S.warpIntoSpawned}, {"freeze_mission_timer", &S.freezeMissionTimer},
    {"p_bullet", &S.playerProofs.bullet}, {"p_fire", &S.playerProofs.fire},
    {"p_explosion", &S.playerProofs.explosion}, {"p_collision", &S.playerProofs.collision},
    {"p_melee", &S.playerProofs.melee}, {"c_bullet", &S.carProofs.bullet}, {"c_fire", &S.carProofs.fire},
    {"c_explosion", &S.carProofs.explosion}, {"c_collision", &S.carProofs.collision},
    {"c_melee", &S.carProofs.melee}, {"overlay_coords", &S.overlayCoords},
    {"overlay_speed", &S.overlaySpeed}, {"overlay_fps", &S.overlayFps}, {"overlay_active", &S.overlayActive},
};
// Freeze time is not restored at startup on purpose: the frozen hour comes from
// the moment it was switched on, which a new session doesn't have.

void LoadSettings() {
    FILE* f = fopen(kIni, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char key[64]; char val[64];
        if (sscanf(line, " %63[^= ] = %63s", key, val) != 2) continue;
        for (auto& b : kBools) if (!strcmp(key, b.key)) *b.val = atoi(val) != 0;
        for (auto& h : HotkeyActions()) {
            if (!strncmp(key, "key_", 4) && h.id == key + 4) h.vk = atoi(val);
            if (!strncmp(key, "keymods_", 8) && h.id == key + 8) h.mods = atoi(val);
        }
        if (!strcmp(key, "mouse_speed")) g_mouseSensitivity = (float)atof(val);
        if (!strcmp(key, "fly_speed")) S.flySpeed = (float)atof(val);
        if (!strcmp(key, "heavy_mode")) S.heavyMode = atoi(val) % 3;
    }
    fclose(f);
}

void SaveSettings() {
    FILE* f = fopen(kIni, "w");
    if (!f) return;
    fprintf(f, "# VC Trainer settings (written by the trainer)\n");
    for (auto& b : kBools) fprintf(f, "%s = %d\n", b.key, *b.val ? 1 : 0);
    for (auto& h : HotkeyActions())
        if (h.vk) fprintf(f, "key_%s = %d\nkeymods_%s = %d\n", h.id.c_str(), h.vk, h.id.c_str(), h.mods);
    fprintf(f, "mouse_speed = %.2f\n", g_mouseSensitivity);
    fprintf(f, "fly_speed = %.1f\n", S.flySpeed);
    fprintf(f, "heavy_mode = %d\n", S.heavyMode);
    fclose(f);
}

bool OverlayEnabled() { return S.enabled && (S.overlayCoords || S.overlaySpeed || S.overlayFps || S.overlayActive); }

// A combo matches only with exactly its modifiers held, so "F" and "Shift + F"
// can be bound to different things.
static bool HotkeyDown(const HotkeyAction& h) {
    if (!h.vk || !KeyHeld(h.vk)) return false;
    return IsModifierKey(h.vk) || CurrentModifiers() == h.mods;
}

static bool GameFocused() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

static void ProcessHotkeys() {
    // Mouse buttons can't be captured through window messages (the game reads
    // the mouse through DirectInput), so poll them while a binding is waiting.
    if (g_capturingHotkey >= 0) {
        const int mouse[] = { VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
        for (int vk : mouse) if (KeyHeld(vk)) { BindCapturedKey(vk, CurrentModifiers()); break; }
    }
    bool active = GameFocused() && !g_menuOpen && g_capturingHotkey < 0;
    for (auto& h : HotkeyActions()) {
        if (!h.vk) { h.prev = false; continue; }
        // With the master switch off, only the master switch hotkey still works.
        bool allowed = S.enabled || h.id == "master";
        bool down = active && allowed && HotkeyDown(h);
        bool pressed = down && !h.prev;
        h.prev = down;
        switch (h.kind) {
        case HotkeyAction::Hold:   if (down) h.fn(); break;
        case HotkeyAction::Press:  if (pressed) h.fn(); break;
        case HotkeyAction::Toggle:
            if (pressed) { *h.toggle = !*h.toggle; ShowNative("%s: %s", h.label.c_str(), *h.toggle ? "ON" : "OFF"); }
            break;
        }
    }
}

// ------------------------------------------------------------------ native cheats
void RunNativeCheat(const char* code) {
    for (const char* c = code; *c; c++) AddToPCCheatString(Pad0, *c);
}

static const CheatFlag* FindCheatFlag(const char* code) {
    for (auto& f : kCheatFlags) if (!strcmp(f.code, code)) return &f;
    return nullptr;
}

bool NativeCheatHasState(const char* code) { return FindCheatFlag(code) != nullptr; }

bool NativeCheatIsOneWay(const char* code) {
    for (auto c : kOneWayCheats) if (!strcmp(c, code)) return true;
    return false;
}

const char* NativeCheatLabel(const char* code) {
    for (auto& g : kCheats)
        for (auto& c : g.c) if (c.code && !strcmp(c.code, code)) return c.desc;
    return code;
}

bool NativeCheatOn(const char* code) {
    const CheatFlag* f = FindCheatFlag(code);
    if (!f) return false;
    switch (f->kind) {
    case 0: case 1: return *(unsigned char*)f->addr != 0;
    case 2: return *(int*)f->addr != 0;
    case 3: {   // fat and skinny share one value: fat is above normal, skinny below
        float v = *(float*)f->addr;
        return strcmp(code, "DEEPFRIEDMARSBARS") == 0 ? (v != 1.0f && v > 0.0f) : v < 0.0f;
    }
    }
    return false;
}

// Writes the flag directly (no "Cheat activated" message, and works for the
// cheats the game itself can only switch on).
void SetNativeCheat(const char* code, bool on) {
    const CheatFlag* f = FindCheatFlag(code);
    if (!f) { if (on) RunNativeCheat(code); return; }
    if (f->kind == 3) {
        if (on) RunNativeCheat(code);          // the game's own value for fat / skinny
        else *(float*)f->addr = 1.0f;          // the exe's initial value = normal
    } else if (f->kind == 2) {
        *(int*)f->addr = on ? 1 : 0;
    } else {
        *(unsigned char*)f->addr = on ? 1 : 0;
        // pink and black traffic exclude each other, as in the game
        if (on && !strcmp(code, "AHAIRDRESSERSCAR")) *(unsigned char*)0xA10B82 = 0;
        if (on && !strcmp(code, "IWANTITPAINTEDBLACK")) *(unsigned char*)0xA10B26 = 0;
    }
    ShowNative("%s: %s", NativeCheatLabel(code), on ? "ON" : "OFF");
}

std::vector<ActiveItem> ActiveCheats() {
    std::vector<ActiveItem> out;
    if (!S.enabled) return out;   // master switch off: nothing from the trainer is running

    for (auto& h : HotkeyActions()) {
        if (h.kind != HotkeyAction::Toggle || !h.toggle || !*h.toggle) continue;
        if (h.id == "warp_into_spawned" || h.category == "Overlay") continue;   // preferences, not cheats
        bool* t = h.toggle;
        out.push_back({ h.label, h.category, [t] { *t = false; } });
    }
    if (FindPlayerPed()) {
        for (auto& f : kCheatFlags) {
            if (!NativeCheatOn(f.code)) continue;
            const char* code = f.code;
            out.push_back({ NativeCheatLabel(code), "Game cheat", [code] { SetNativeCheat(code, false); } });
        }
        if (*ForcedWeather >= 0) {
            const char* names[] = { "Sunny", "Cloudy", "Rain", "Fog", "Extra sunny", "Hurricane", "Extra colours" };
            int w = *ForcedWeather;
            out.push_back({ std::string("Weather locked: ") + (w < 7 ? names[w] : "?"), "World", [] { ReleaseWeather(); } });
        }
        if (fabsf(*TimeScale - 1.0f) > 0.01f) {
            char b[48]; snprintf(b, sizeof b, "Game speed %.2fx", *TimeScale);
            out.push_back({ b, "World", [] { *TimeScale = 1.0f; } });
        }
        if (fabsf(*PedDensity - 1.0f) > 0.01f) {
            char b[48]; snprintf(b, sizeof b, "Pedestrians %.2fx", *PedDensity);
            out.push_back({ b, "World", [] { *PedDensity = 1.0f; } });
        }
        if (fabsf(*CarDensity - 1.0f) > 0.01f) {
            char b[48]; snprintf(b, sizeof b, "Traffic %.2fx", *CarDensity);
            out.push_back({ b, "World", [] { *CarDensity = 1.0f; } });
        }
    }
    return out;
}

void TurnEverythingOff() {
    for (auto& a : ActiveCheats()) a.turnOff();
    ShowNative("All cheats turned off");
}

// ------------------------------------------------------------------ fly mode
// Each frame (after the game's own physics) the player - or the vehicle they
// are in - is put at a position the trainer controls, so walls, gravity and
// collisions have no effect. Direction follows the camera.
static void FlyTick() {
    const Settings& A = Active();
    static bool active = false;
    static void* ent = nullptr;
    static float pos[3];
    static LARGE_INTEGER freq, last;

    void* ped = PlayerPed();
    void* target = PlayerVehicle(); if (!target) target = ped;
    if (!A.flyMode || !target) { active = false; return; }

    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    if (!active || ent != target) {            // just switched on, or got in/out of a car
        active = true; ent = target; last = now;
        memcpy(pos, &at<float>(ent, ENT_POS), 12);
    }
    float dt = (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart;
    last = now;
    if (dt > 0.1f) dt = 0.1f;

    // camera look direction and a level "right" vector
    float f[3] = { CameraFront[0], CameraFront[1], CameraFront[2] };
    float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (fl < 0.001f) { f[0] = 0; f[1] = 1; f[2] = 0; fl = 1; }
    for (float& c : f) c /= fl;
    float hl = sqrtf(f[0] * f[0] + f[1] * f[1]);
    float hx = hl > 0.001f ? f[0] / hl : 0.0f, hy = hl > 0.001f ? f[1] / hl : 1.0f;
    float r[3] = { hy, -hx, 0 };

    float mv[3] = { 0, 0, 0 };
    if (GameFocused() && !g_menuOpen) {
        float fwd = (KeyHeld('W') ? 1.0f : 0.0f) - (KeyHeld('S') ? 1.0f : 0.0f);
        float side = (KeyHeld('D') ? 1.0f : 0.0f) - (KeyHeld('A') ? 1.0f : 0.0f);
        float up = (KeyHeld(VK_SPACE) ? 1.0f : 0.0f) - (KeyHeld(VK_CONTROL) ? 1.0f : 0.0f);
        for (int i = 0; i < 3; i++) mv[i] = f[i] * fwd + r[i] * side;
        mv[2] += up;
        float speed = A.flySpeed * (KeyHeld(VK_SHIFT) ? 3.0f : 1.0f);
        for (int i = 0; i < 3; i++) pos[i] += mv[i] * speed * dt;
    }

    // Move the entity to the new spot and re-file it in the world sectors so it
    // keeps rendering and streaming correctly wherever it goes.
    EntityRemove(ent);
    memcpy(&at<float>(ent, ENT_POS), pos, 12);
    float* m = &at<float>(ent, ENT_RIGHT);  m[0] = hy; m[1] = -hx; m[2] = 0;   // face the camera
    float* fw = &at<float>(ent, ENT_FWD);   fw[0] = hx; fw[1] = hy; fw[2] = 0;
    float* u = &at<float>(ent, ENT_UP);     u[0] = 0; u[1] = 0; u[2] = 1;
    EntityAdd(ent);
    memset(&at<float>(ent, PHYS_MOVE), 0, 12);
    memset(&at<float>(ent, PHYS_TURN), 0, 12);
}

// ------------------------------------------------------------------ per frame
void TrainerTick() {
    const Settings& A = Active();
    // run what the menu asked for
    if (!g_queue.empty()) {
        auto q = std::move(g_queue);
        g_queue.clear();
        for (auto& fn : q) fn();
    }

    void* ped = PlayerPed();
    if (!ped) return;

    ProcessHotkeys();
    FlyTick();

    // These two are game flags the script also uses, so only force them while on.
    static bool lastTired = false, lastReload = false;
    if (A.neverTired) *NeverTired = 1; else if (lastTired) *NeverTired = 0;
    if (A.fastReload) *FastReload = 1; else if (lastReload) *FastReload = 0;
    lastTired = A.neverTired; lastReload = A.fastReload;

    if (A.godMode) {
        float maxH = (float)*MaxHealth;
        if (at<float>(ped, PED_HEALTH) < maxH) at<float>(ped, PED_HEALTH) = maxH;
    }
    if (A.infiniteArmour) {
        float maxA = (float)*MaxArmour;
        if (at<float>(ped, PED_ARMOUR) < maxA) at<float>(ped, PED_ARMOUR) = maxA;
    }
    if (A.neverWanted && GetWanted() > 0) SetWanted(0);

    if (A.infiniteAmmo || A.noReload) {
        for (int i = 0; i < WEAPON_SLOTS; i++) {
            char* w = (char*)ped + PED_WEAPONS + i * WEAPON_SIZE;
            int type = *(int*)(w + WEAP_TYPE);
            if (type <= 0) continue;
            // The detonator (given after throwing remote grenades) has 1 charge;
            // using it drops that to 0 and the game swaps back to your weapon.
            // Refilling it left Tommy stuck pressing the button, and giving a
            // weapon in that state crashed the game. Never refill it, and repair
            // a detonator a previous version already over-filled.
            if (type == WEAPON_DETONATOR) {
                if (*(int*)(w + WEAP_TOTAL) > 1) *(int*)(w + WEAP_TOTAL) = 1;
                if (*(int*)(w + WEAP_CLIP) > 1) *(int*)(w + WEAP_CLIP) = 1;
                continue;
            }
            char* info = GetWeaponInfo(type);
            if (*(int*)(info + WI_FIRETYPE) == 0) continue; // melee
            if (A.infiniteAmmo && *(int*)(w + WEAP_TOTAL) < 9000) *(int*)(w + WEAP_TOTAL) = 9999;
            int clip = *(int*)(info + WI_CLIP);
            if (A.noReload && clip > 1 && *(int*)(w + WEAP_CLIP) < clip) *(int*)(w + WEAP_CLIP) = clip;
        }
    }

    UnlockOnExit();   // safety fix: always on, even with the master switch off
    HeavyCarTick(A);
    StayOnBikeTick(A);
    BikeExitFix(A);

    static Proofs lastPed, lastCar;
    static void* lastCarPtr = nullptr;
    ApplyProofs(ped, A.playerProofs, lastPed, false);

    if (void* veh = PlayerVehicle()) {
        if (veh != lastCarPtr) { lastCar = Proofs(); lastCarPtr = veh; }
        ApplyProofs(veh, A.carProofs, lastCar, false);
        if (A.carGodMode && at<float>(veh, VEH_HEALTH) < 1000.0f) at<float>(veh, VEH_HEALTH) = 1000.0f;
        if (A.autoRepair && at<float>(veh, VEH_HEALTH) < 990.0f) RepairVehicle();
    } else {
        lastCarPtr = nullptr;
    }

    if (A.freezeTime) {
        *ClockHours = (unsigned char)A.frozenHour;
        *ClockMinutes = (unsigned char)A.frozenMinute;
    }

    if (A.freezeMissionTimer) {
        static unsigned frozenOff = 0; static int frozenVal = 0;
        unsigned off = *OnscnTimerOffset;
        if (!off) frozenOff = 0;
        else {
            int* v = (int*)(ScriptSpace + off);
            if (off != frozenOff) { frozenOff = off; frozenVal = *v; }
            else if (*v < frozenVal) *v = frozenVal;   // block the countdown
            else frozenVal = *v;
        }
    }
}
