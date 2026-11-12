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

static DWORD g_killUntil = 0;    // suicide in progress: protections are suspended until then

static DWORD g_blastSafeUntil = 0;  // just put out of an exploding vehicle

const Settings& Active() {
    static Settings off = [] { Settings s; s.enabled = false; s.warpIntoSpawned = false; return s; }();
    const Settings* cur = &S;
    if (!S.enabled) { off.flySpeed = S.flySpeed; cur = &off; }
    if (g_blastSafeUntil && GetTickCount() < g_blastSafeUntil && !(g_killUntil && GetTickCount() < g_killUntil)) {
        // Thrown clear of an exploding vehicle: the blast and the fire that
        // follow can't kill the player for a few seconds.
        static Settings b;
        b = *cur;
        b.godMode = true;
        b.playerProofs.explosion = b.playerProofs.fire = true;
        return b;
    }
    if (g_killUntil && GetTickCount() < g_killUntil) {
        // Suicide: everything that would keep the player alive is off.
        static Settings k;
        k = *cur;
        k.godMode = k.infiniteArmour = k.stayOnBike = k.flyMode = false;
        k.playerProofs = Proofs();
        return k;
    }
    return *cur;
}

void SetTrainerEnabled(bool on) {
    S.enabled = on;
    ShowNative("Modern VC Trainer %s", on ? "ON" : "OFF - all cheats paused");
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
// The chosen max is remembered in the settings and put back every frame the
// game's value is lower (new game, loaded save, restart of the game).
void SetMaxHealth(int v) { v = v < 1 ? 1 : v > 255 ? 255 : v; *MaxHealth = (unsigned char)v; S.maxHealth = v; }
void SetMaxArmour(int v) { v = v < 1 ? 1 : v > 255 ? 255 : v; *MaxArmour = (unsigned char)v; S.maxArmour = v; }

// Suicide that always works: for a few seconds god mode, infinite armour,
// proofs and never-fall-off-bike are suspended (see Active), armour is
// removed and the game's own damage kills the player - on foot or in any
// vehicle. KillTick keeps health at 0 until the game has the player dying.
void KillPlayer() { g_killUntil = GetTickCount() + 15000; }

static void KillTick(void* ped) {
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
bool NoRagdollOn() { return Active().noRagdoll; }

int WantedCap() {
    const Settings& A = Active();
    if (!A.wantedCapOn) return -1;
    return A.wantedCap < 0 ? 0 : A.wantedCap > 6 ? 6 : A.wantedCap;
}

bool HoldNoWanted() {
    return Active().neverWanted || (g_noWantedUntil && GetTickCount() < g_noWantedUntil);
}

void SetWanted(int level) {
    void* p = PlayerPed(); if (!p) return;
    if (level <= 0) g_noWantedUntil = GetTickCount() + 3000;
    else g_noWantedUntil = 0;                       // raising it on purpose
    if (void* w = at<void*>(p, PED_WANTED)) game::SetWantedLevel(w, level < 0 ? 0 : level);
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

// ---- taking an occupied police vehicle while never wanted ------------------
// Normally trying to get into a manned police car makes the cops jump out
// after you (the attempted theft raises the wanted level), which frees the
// car. With the wanted level held at zero that never happens and the car
// stays shut. So: when the player tries to get into a vehicle driven by a
// cop, everyone inside is told to get out, and once the
// driver's seat is free the player is sent in, the game's normal way.
static bool IsLiveVehicle(void* p);
static bool IsCopModel(int m) { return (m >= 1 && m <= 4) || (m >= 97 && m <= 104); }  // cop swat fbi army, vice1-8

static void CopCarEntryTick() {
    static void* target = nullptr;
    static DWORD since = 0;
    static bool prevBtn = false;
    static DWORD pressedAt = 0;
    DWORD now = GetTickCount();
    // The enter key, tracked every frame (also in a vehicle) so holding it
    // while getting out never reads as a new press afterwards.
    bool btn = at<short>(Pad0, 0x1E) != 0;
    if (btn && !prevBtn && !g_menuOpen && at<short>(Pad0, 0xF0) == 0) pressedAt = now;
    prevBtn = btn;
    void* ped = PlayerPed();
    if (!ped || PlayerVehicle() || !HoldNoWanted()) { target = nullptr; pressedAt = 0; return; }

    // Only an actual attempt to open that police car: the enter key pressed
    // in the last half second and the game's "enter this car" objective
    // pointing at it. Walking into or brushing against a police car does
    // nothing.
    bool triedNow = pressedAt && now - pressedAt < 500;
    int obj = at<int>(ped, PED_OBJECTIVE);
    void* car = (obj == 0x12 || obj == 0x11) ? at<void*>(ped, PED_CAR_IN_OBJECTIVE) : nullptr;
    if (!target && triedNow && car && IsLiveVehicle(car) && at<float>(car, VEH_HEALTH) > 0.0f) {
        void* drv = at<void*>(car, VEH_DRIVER);
        if (drv && drv != ped && IsCopModel(at<short>(drv, ENT_MODEL))) {
            target = car; since = now; pressedAt = 0;
        }
    }
    if (!target) return;
    if (!IsLiveVehicle(target) || now - since > 5000) { target = nullptr; return; }

    void** seats = &at<void*>(target, VEH_DRIVER);       // driver + 8 passengers
    bool anyone = false;
    for (int i = 0; i < 9; i++) {
        void* occ = seats[i];
        if (!occ || occ == ped) continue;
        anyone = true;
        if (at<int>(occ, PED_STATE) == 0x32 || at<int>(occ, PED_STATE) == 0x33)   // still sitting: get out
            PedSetObjective(occ, 0x10, target);                                   // OBJECTIVE_LEAVE_CAR
    }
    if (!seats[0] && (!anyone || now - since > 600)) {     // driver's seat is free: in we go
        PedSetObjective(ped, 0x12, target);                // OBJECTIVE_ENTER_CAR_AS_DRIVER
        target = nullptr;
    }
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

// Full repair: health, fire, engine, tyres, doors, panels, lights and the
// bodywork model, the same way the game repairs a car.
static void FixVehicle(void* veh) {
    at<float>(veh, VEH_HEALTH) = 1000.0f;
    int type = at<int>(veh, VEH_TYPE);
    if (type == 0)      { at<float>(veh, AUTO_FIRE_TIMER) = 0.0f; AutomobileFix(veh); }
    else if (type == 5) { at<float>(veh, BIKE_FIRE_TIMER) = 0.0f; BikeFix(veh); }
}

// Anything a repair would change: lost health, a burst tyre, a dented panel,
// a broken light or a damaged door (cars without doors keep theirs "missing").
static bool VehicleDamaged(void* veh) {
    if (at<float>(veh, VEH_HEALTH) < 1000.0f) return true;
    if (at<unsigned char>(veh, VEH_FLAGS_1FB) & 0x02) return true;
    int type = at<int>(veh, VEH_TYPE);
    if (type == 5) return at<unsigned short>(veh, BIKE_TYRES) != 0;
    if (type != 0) return false;
    const unsigned char* dm = (const unsigned char*)veh + VEH_DAMAGE;
    for (int i = 4; i < 9; i++) if (dm[i]) return true;               // engine, wheels
    void* h = at<void*>(veh, VEH_HANDLING);
    bool noDoors = h && (at<int>(h, HANDLING_FLAGS) & HANDLING_NO_DOORS);
    for (int d = 0; d < 6; d++) {                                      // bonnet, boot, 4 doors
        unsigned char want = (noDoors && d >= 2) ? 3 : 0;
        if (dm[9 + d] != want) return true;
    }
    return *(const unsigned*)(dm + 0x10) || *(const unsigned*)(dm + 0x14); // lights, panels
}

static bool VehicleWrecked(void* veh) {
    return at<float>(veh, VEH_HEALTH) <= 0.0f || (at<unsigned char>(veh, ENT_STATUS) >> 3) == 5;
}

void RepairVehicle() {
    void* veh = PlayerVehicle(); if (!veh || VehicleWrecked(veh)) return;
    FixVehicle(veh);
}

// Sets the vehicle upright, facing the same way. Its speed along the ground is
// kept (only the spin is stopped), so flipping while moving doesn't stop dead.
static void UprightVehicle(void* veh) {
    float* f = &at<float>(veh, ENT_FWD);
    float* u = &at<float>(veh, ENT_UP);
    float fx = f[0], fy = f[1], len = sqrtf(fx * fx + fy * fy);
    if (len < 0.01f) { fx = 0; fy = 1; } else { fx /= len; fy /= len; }
    // Lift it clear of the ground: a lot when it was on its roof or side, a
    // little when it was only leaning, so the wheels land instead of clipping.
    float lift = u[2] < 0.5f ? 1.5f : 0.3f;
    EntityRemove(veh);
    float* r = &at<float>(veh, ENT_RIGHT); r[0] = fy; r[1] = -fx; r[2] = 0;
    f[0] = fx; f[1] = fy; f[2] = 0;
    u[0] = 0; u[1] = 0; u[2] = 1;
    at<float>(veh, ENT_POS + 8) += lift;
    float* mv = &at<float>(veh, PHYS_MOVE);
    if (mv[2] < 0.0f) mv[2] = 0.0f;                 // keep x/y speed, no downward slam
    memset(&at<float>(veh, PHYS_TURN), 0, 12);
    EntityAdd(veh);
}

void FlipVehicle() {
    void* veh = PlayerVehicle(); if (!veh) return;
    UprightVehicle(veh);
}

// ---- survive vehicle explosions ------------------------------------------
// An exploding vehicle kills everyone inside with CPed::SetDead, which ignores
// health, god mode and proofs. The explosion hook (hooks.cpp) calls this just
// before that happens: the player is taken out of the vehicle the way the
// game's own script command 0362 does it, set down on the ground beside the
// driver's door, and protected from the blast and fire for a few seconds.
bool SurviveBlowUpOn() { return S.enabled && S.surviveBlowUp && !(g_killUntil && GetTickCount() < g_killUntil); }

bool BlowUpStayInside() { return S.blowUpMode == 0; }

static void ProtectFromBlast(void* ped) {
    g_blastSafeUntil = GetTickCount() + 4000;
    // The explosion is added right after the kill step, before the next
    // trainer tick applies the window's proofs, so set them on the ped now
    // (the tick switches them back off after the window unless they're on).
    at<unsigned char>(ped, PHYS_FLAGS1) |= 0x02;       // explosion proof
    at<unsigned char>(ped, PHYS_FLAGS2) |= 0x04;       // fire proof
    if (at<float>(ped, PED_HEALTH) < 100.0f) at<float>(ped, PED_HEALTH) = 100.0f;
}

// Stay-inside mode: the seat (driver or passenger slot) the player sits in,
// so the hook can leave it empty only while the game kills the occupants.
void** PlayerSeatInBlast(void* veh) {
    void* ped = FindPlayerPed();
    if (!ped || !veh || at<void*>(ped, PED_MY_VEHICLE) != veh || !at<unsigned char>(ped, PED_IN_VEHICLE_FLAG)) return nullptr;
    void** seats = &at<void*>(veh, VEH_DRIVER);                 // driver, then 8 passengers
    for (int i = 0; i < 9; i++)
        if (seats[i] == ped) { ProtectFromBlast(ped); return &seats[i]; }
    return nullptr;
}

static void WarpPlayerOut(void* veh, void* ped);

void EjectPlayerFromBlast(void* veh) {
    void* ped = FindPlayerPed();
    if (!ped || !veh || at<void*>(ped, PED_MY_VEHICLE) != veh || !at<unsigned char>(ped, PED_IN_VEHICLE_FLAG)) return;
    ProtectFromBlast(ped);
    WarpPlayerOut(veh, ped);
}

// Sitting in a wreck: kept safe from its fire, and the exit button always
// works - if the game won't start getting out within 0.3 s, the player is
// set down beside it.
static void WreckTick(const Settings& A) {
    static bool prev = false;
    static DWORD pressAt = 0;
    void* ped = PlayerPed();
    void* veh = PlayerVehicle();
    bool btn = at<short>(Pad0, 0x1E) != 0;
    bool pressed = btn && !prev;
    prev = btn;
    bool wreck = veh && at<unsigned char>(ped, PED_IN_VEHICLE) &&
                 (at<float>(veh, VEH_HEALTH) <= 0.0f || (at<unsigned char>(veh, ENT_STATUS) >> 3) == 5);
    if (!wreck || !A.enabled || !S.surviveBlowUp) { pressAt = 0; return; }
    ProtectFromBlast(ped);
    if (pressed && !g_menuOpen && at<short>(Pad0, 0xF0) == 0) pressAt = GetTickCount();
    if (pressAt && GetTickCount() - pressAt > 300) {
        pressAt = 0;
        if (at<void*>(ped, PED_MY_VEHICLE) == veh && at<int>(ped, PED_STATE) == 0x32) WarpPlayerOut(veh, ped);
    }
}

static void WarpPlayerOut(void* veh, void* ped) {

    // where to stand: 3 m out from the driver's (left) side, on the ground
    const float* vp = &at<float>(veh, ENT_POS);
    const float* r = &at<float>(veh, ENT_RIGHT);
    float pos[3] = { vp[0] - r[0] * 3.0f, vp[1] - r[1] * 3.0f, vp[2] };
    bool found = false;
    float gz = FindGroundZ3D(pos[0], pos[1], pos[2] + 2.0f, &found);
    if (found && gz > pos[2] - 6.0f) pos[2] = gz;

    if (at<void*>(veh, VEH_DRIVER) == ped) {
        VehRemoveDriver(veh);
        if ((at<unsigned char>(veh, ENT_STATUS) >> 3) != 5)                                   // not a wreck:
            at<unsigned char>(veh, ENT_STATUS) = (at<unsigned char>(veh, ENT_STATUS) & 7) | 0x20;  // abandoned
        at<unsigned char>(veh, 0x1F9) &= ~0x10;                                               // engine off
        at<unsigned char>(veh, VEH_CRUISE_SPEED) = 0;
    } else {
        VehRemovePassenger(veh, ped);
    }
    Ped_512440(ped, 1);
    at<unsigned char>(ped, PED_IN_VEHICLE_FLAG) = 0;
    at<void*>(ped, PED_MY_VEHICLE) = nullptr;
    if (at<int>(ped, PED_STATE) == 0xB) Ped_4F7920(ped);
    at<int>(ped, PED_STATE) = 1;                                    // PED_IDLE
    at<unsigned char>(ped, 0x51) |= 1;                              // uses collision again
    memset(&at<float>(ped, PHYS_MOVE), 0, 12);
    Ped_4FF5A0(ped);
    if (void* anim = at<void*>(ped, PED_VEHICLE_ANIM)) at<float>(anim, 0x1C) = -1000.0f;
    at<void*>(ped, PED_VEHICLE_ANIM) = nullptr;
    Ped_50D110(ped, 0);
    BlendAnimation(at<void*>(ped, ENT_CLUMP), at<int>(ped, PED_ANIM_GROUP), 3, 1000.0f);  // standing idle
    Ped_50CCF0(ped);
    pos[2] += PedDistToBase(ped);
    using Teleport_t = void(__thiscall*)(void*, float, float, float);
    ((Teleport_t)(*(void***)ped)[11])(ped, pos[0], pos[1], pos[2]);   // CEntity::Teleport
    ClearSpaceForMissionEntity(pos, ped);
}

// Auto flip (cars only, not bikes/boats/helis/planes): as soon as the player's
// car is upside down AND touching the ground, turn it back over - it may still
// be sliding, and keeps that speed. In the air it is never flipped: the ground
// below the car (traced down from the car, so bridges above don't count) must
// be within reach of the roof.
static void AutoFlipTick(const Settings& A) {
    static DWORD invertedSince = 0, lastFlip = 0;
    void* veh = PlayerVehicle();
    // VEH_TYPE 0 = CAutomobile, which also covers helis/planes; the model info
    // type (0 car, 1 boat, 3 heli, 4 plane, 5 bike) narrows it to real cars.
    bool car = veh && at<int>(veh, VEH_TYPE) == 0 &&
               at<int>(ModelInfoPtrs[at<short>(veh, ENT_MODEL)], MI_VEHTYPE) == 0;
    if (!A.autoFlip || !car || at<float>(veh, VEH_HEALTH) <= 0.0f) {
        invertedSince = 0; return;
    }
    if (at<float>(veh, ENT_UP + 8) >= -0.2f) { invertedSince = 0; return; }   // wheels not up

    const float* p = &at<float>(veh, ENT_POS);
    bool found = false;
    float ground = FindGroundZ3D(p[0], p[1], p[2] + 0.5f, &found);
    // Car centre is about 0.5-1 m above the ground when it lies on its roof.
    bool onGround = found && p[2] - ground < 1.8f && at<float>(veh, PHYS_MOVE + 8) > -0.15f;
    DWORD now = GetTickCount();
    if (!onGround) { invertedSince = 0; return; }
    if (!invertedSince) invertedSince = now;
    // 0.1 s on the roof filters out a car only rolling through upside down for
    // a single frame; 1 s between flips stops repeated flipping.
    if (now - invertedSince < 100 || now - lastFlip < 1000) return;
    UprightVehicle(veh);
    lastFlip = now;
    invertedSince = 0;
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

    char* pool = (char*)*VehiclePool; if (!pool) return;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    float* p = &at<float>(veh, ENT_POS);
    float* r = &at<float>(veh, ENT_RIGHT);
    float* f = &at<float>(veh, ENT_FWD);
    DWORD now = GetTickCount();

    // Real touches: the game's collision records (CPhysical: count +0xE6,
    // up to 6 entities from +0xE8), checked on both cars.
    auto touched = [](void* a, void* b) {
        int n = at<unsigned char>(a, PHYS_NUM_COLLISIONS);
        if (n > PHYS_MAX_COLLISIONS) n = PHYS_MAX_COLLISIONS;
        for (int k = 0; k < n; k++)
            if ((&at<void*>(a, PHYS_COLLISIONS))[k] == b) return true;
        return false;
    };

    for (int i = 0; i < size; i++) {
        if (flags[i] & 0x80) continue;
        void* o = objs + i * VEHICLE_POOL_STRIDE;
        if (o == veh || at<float>(o, VEH_HEALTH) <= 0.0f) continue;   // self, or already wrecked
        if ((at<unsigned char>(o, ENT_STATUS) >> 3) == 5) continue;  // wreck
        float* op = &at<float>(o, ENT_POS);
        float d[3] = { op[0] - p[0], op[1] - p[1], op[2] - p[2] };
        if (fabsf(d[0]) > 30.0f || fabsf(d[1]) > 30.0f || fabsf(d[2]) > 30.0f) continue;
        bool hit = touched(veh, o) || touched(o, veh);
        if (!hit && mySpeed >= 0.03f && fabsf(d[2]) <= 3.0f) {
            // moving fast, the records can miss a glancing hit: also count
            // anything right against the player car's box
            float side = d[0] * r[0] + d[1] * r[1], ahead = d[0] * f[0] + d[1] * f[1];
            hit = fabsf(side) <= 2.8f && fabsf(ahead) <= 4.8f;
        }
        if (!hit) continue;

        bool recent = false;
        for (auto& h : hitTimes) if (h.first == o && now - h.second < 1500) recent = true;
        if (recent) continue;
        hitTimes.push_back({ o, now });

        if (A.heavyMode == 2) {
            // any vehicle at all: mission cars and damage-proof cars too
            at<unsigned char>(o, VEH_FLAGS_1FB) |= 0x40;            // bCanBeDamaged
            at<unsigned char>(o, PHYS_FLAGS1) &= ~0x02;             // not explosion proof
            at<float>(o, VEH_HEALTH) = 0.0f;
            VehicleBlowUp(o, PlayerPed());
        } else {
            float side = d[0] * r[0] + d[1] * r[1];
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

// ---- kill peds by touch -----------------------------------------------------
// Vehicle option: a ped dies the moment it touches the vehicle the player is
// driving - the game's collision records (either side) or being pressed
// against the vehicle's box. Peds sitting in vehicles are left alone, as are
// the player's own passengers.
static bool InCollisionRecords(void* a, void* b) {
    int n = at<unsigned char>(a, PHYS_NUM_COLLISIONS);
    if (n > PHYS_MAX_COLLISIONS) n = PHYS_MAX_COLLISIONS;
    for (int k = 0; k < n; k++)
        if ((&at<void*>(a, PHYS_COLLISIONS))[k] == b) return true;
    return false;
}

static void TouchKillTick(const Settings& A) {
    if (!A.touchKill) return;
    void* me = PlayerPed(); if (!me) return;
    void* veh = PlayerVehicle(); if (!veh) return;      // vehicle feature only
    void* body = veh;
    char* pool = (char*)*PedPool; if (!pool) return;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    const float* p = &at<float>(body, ENT_POS);
    const float* r = &at<float>(body, ENT_RIGHT);
    const float* f = &at<float>(body, ENT_FWD);

    for (int i = 0; i < size; i++) {
        if (flags[i] & 0x80) continue;
        void* o = objs + i * PED_POOL_STRIDE;
        if (o == me) continue;
        int st = at<int>(o, PED_STATE);
        if (st == PED_DIE || st == PED_DEAD || at<float>(o, PED_HEALTH) <= 0.0f) continue;
        if (at<unsigned char>(o, PED_IN_VEHICLE)) continue;               // sitting in a vehicle
        if (veh && at<void*>(o, PED_MY_VEHICLE) == veh) continue;          // getting in/out of ours
        const float* op = &at<float>(o, ENT_POS);
        float d[3] = { op[0] - p[0], op[1] - p[1], op[2] - p[2] };
        if (fabsf(d[0]) > 15.0f || fabsf(d[1]) > 15.0f || fabsf(d[2]) > 15.0f) continue;
        bool hit = InCollisionRecords(body, o) || InCollisionRecords(o, body);
        if (!hit && fabsf(d[2]) < 2.0f) {   // against the vehicle's box (a car is ~2 m wide, ~5 m long)
            float side = d[0] * r[0] + d[1] * r[1], ahead = d[0] * f[0] + d[1] * f[1];
            hit = fabsf(side) <= 1.6f && fabsf(ahead) <= 3.0f;
        }
        if (!hit) continue;
        InflictDamage(o, me, 45 /* UNIDENTIFIED */, 1000.0f, 0, 0);
        at<float>(o, PED_ARMOUR) = 0.0f;
        at<float>(o, PED_HEALTH) = 0.0f;
    }
}

static bool GameFocused();
static bool IsLivePed(void* p) {
    char* pool = (char*)*PedPool; if (!pool || !p) return false;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    int off = (int)((char*)p - objs);
    if (off < 0 || off % PED_POOL_STRIDE) return false;
    int i = off / PED_POOL_STRIDE;
    return i < size && !(flags[i] & 0x80);
}

// ---- aim lock-on --------------------------------------------------------------
// Only while the player is aiming (right mouse button held, on foot) and an
// enemy - someone trying to kill the player, or police when wanted - is
// anywhere around the player (360 degrees). The camera is then turned so the game's own
// crosshair (CCamera::Find3rdPersonCamTargetVector) sits on the enemy's chest.
// Letting go of aim, moving the mouse hard, or the enemy dying / going behind
// a wall releases it. Nobody hostile near the crosshair: nothing happens.
// Where to shoot a ped: the chest when standing, the body on the ground when
// knocked down (falling / lying / getting up) so the lock follows them down.
static void EnemyAimPoint(void* p, float* out) {
    const float* pp = &at<float>(p, ENT_POS);
    out[0] = pp[0]; out[1] = pp[1]; out[2] = pp[2] + 0.45f;
    int st = at<int>(p, PED_STATE);
    if (st == PED_FALL || st == PED_GETUP) {
        bool f = false;
        float g = FindGroundZ3D(pp[0], pp[1], pp[2] + 0.5f, &f);
        out[2] = f ? g + 0.25f : pp[2] - 0.3f;
    }
}

// Police helicopters (CHeli) are their own class: the game keeps them in
// CHeli::pHelis[4]; one is "shot down" with status 3 and an explosion time.
static bool IsHeliTarget(void* h) {
    if (!h) return false;
    bool listed = false;
    for (int i = 0; i < HELI_COUNT; i++) if (HeliList[i] == h) listed = true;
    return listed && at<unsigned char>(h, HELI_STATUS) != HELI_SHOT_DOWN;
}
static void ShootDownHeli(void* h) {
    at<unsigned char>(h, HELI_STATUS) = HELI_SHOT_DOWN;
    at<unsigned>(h, HELI_EXPLODE_AT) = *TimeMs;                               // and right now
}

static bool IsHostile(void* p, void* me) {
    if (!p || p == me) return false;
    int st = at<int>(p, PED_STATE);
    if (st == PED_DIE || st == PED_DEAD || at<float>(p, PED_HEALTH) <= 0.0f) return false;
    int obj = at<int>(p, PED_OBJECTIVE);
    // the player's own people: following the player, or riding with them
    if (obj == OBJ_FOLLOW_CHAR_IN_FORMATION && at<void*>(p, PED_PED_IN_OBJECTIVE) == me) return false;
    if (void* mv = PlayerVehicle())
        if (at<unsigned char>(p, PED_IN_VEHICLE) && at<void*>(p, PED_MY_VEHICLE) == mv) return false;
    // anyone who may turn on the player, angry or not: every gang, the
    // police / SWAT / FBI / army, and criminals
    int type = at<int>(p, PED_TYPE);
    if (type == 6 || (type >= 7 && type <= 15) || type == 18) return true;
    // ped types that hate the player: the riot and "peds attack you" cheats
    // turn this on for everyone, angry gangs have it anyway
    if (type >= 0 && type < PEDTYPE_COUNT) {
        void* pt = PedTypeTable[type];
        if (pt && (at<unsigned>(pt, PEDTYPE_THREATS) & THREAT_PLAYER1)) return true;
    }
    if (IsCopModel(at<short>(p, ENT_MODEL))) return true;
    if ((obj == OBJ_KILL_CHAR_ON_FOOT || obj == OBJ_KILL_CHAR_ANY_MEANS) && at<void*>(p, PED_PED_IN_OBJECTIVE) == me)
        return true;
    if (IsCopModel(at<short>(p, ENT_MODEL)) && GetWanted() > 0) return true;
    // attacking, fighting or pointing a gun - and turned towards the player
    if (st == PED_ATTACK || st == PED_FIGHT || st == PED_AIM_GUN) {
        const float* pp = &at<float>(p, ENT_POS);
        const float* mp = &at<float>(me, ENT_POS);
        const float* f = &at<float>(p, ENT_FWD);
        float dx = mp[0] - pp[0], dy = mp[1] - pp[1];
        float d = sqrtf(dx * dx + dy * dy), fl = sqrtf(f[0] * f[0] + f[1] * f[1]);
        if (d < 50.0f && fl > 0.01f && (dx * f[0] + dy * f[1]) / (d * fl + 0.001f) > 0.8f) return true;   // within ~37 degrees
    }
    return false;
}

// Where the player's shots go: from the camera through the crosshair - the
// game's own third-person crosshair (CCamera::Find3rdPersonCamTargetVector),
// or the screen centre in first-person weapon views (sniper, rocket
// launcher, M4/M60 first person, heli cannon, camera).
static bool CrosshairRay(void* me, float* src, float* dir) {
    const float* mp = &at<float>(me, ENT_POS);
    float tgt[3];
    Find3rdPersonCamTargetVector(TheCamera, 100.0f, mp[0], mp[1], mp[2], src, tgt);
    short mode = at<short>(ActiveCam(), CAM_MODE);
    bool firstPerson = mode == 7 || mode == 8 || mode == 16 || mode == 34 || (mode >= 39 && mode <= 42) ||
                       mode == 45 || mode == 46;
    if (firstPerson) {
        memcpy(src, CameraPos, 12);
        for (int i = 0; i < 3; i++) tgt[i] = src[i] + CameraFront[i] * 100.0f;
    }
    for (int i = 0; i < 3; i++) dir[i] = tgt[i] - src[i];
    float l = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (l < 0.001f) return false;
    for (int i = 0; i < 3; i++) dir[i] /= l;
    return true;
}

static bool g_flyingOnFoot = false;   // fly mode on, on foot (set by FlyTick)
static void PushProjectilesOut(void* ped);

// Can a shot reach this ped? A clear line (fences / glass don't count) from
// the camera or from the player's own eyes to the ped's head, chest or legs.
static void EnemyAimPoint(void* p, float* out);
static bool PedReachable(void* me, const float* camSrc, void* p) {
    const float* tp = &at<float>(p, ENT_POS);
    const float* mp = &at<float>(me, ENT_POS);
    float chest[3]; EnemyAimPoint(p, chest);
    float head[3] = { tp[0], tp[1], tp[2] + 0.7f }, legs[3] = { tp[0], tp[1], tp[2] - 0.5f };
    float eye[3] = { mp[0], mp[1], mp[2] + 0.7f };
    auto clear = [](const float* a, const float* b) {
        return LineOfSightClear(a, b, true, false, false, false, false, true, true);
    };
    return clear(camSrc, chest) || clear(camSrc, head) || clear(eye, chest) || clear(eye, head) || clear(eye, legs);
}

static void* g_aimTarget = nullptr;   // who the aim lock is on (for the enemy markers)
static bool g_bhopping = false;       // a bunny-hop chain is running (BhopTick)
static float g_aimError = 10.0f;      // angle between crosshair and that target, radians

// Weapons that normally need the right button held to shoot (a scope or the
// launcher's sight): sniper, laser sniper, rocket launcher.
static bool IsScopeWeapon(int t) { return t == WEAPON_SNIPER || t == WEAPON_LASER_SNIPER || t == WEAPON_ROCKET_LAUNCHER; }
static bool ScopedNow() {                                 // sniper / rocket camera up
    short m = at<short>(ActiveCam(), CAM_MODE);
    return m == 7 || m == 39 || m == 8 || m == 40;
}

// CWeapon::Fire for any view: the player's rocket launcher only fires with the
// camera in a scope / sight mode, so it gets rocket-sight mode for the call.
static bool FireAnyView(char* w, void* me) {
    if (at<int>(w, WEAP_TYPE) != WEAPON_ROCKET_LAUNCHER || ScopedNow()) return WeaponFire(w, me, nullptr);
    short* mode = &at<short>(ActiveCam(), CAM_MODE);
    short oldMode = *mode;
    *mode = 8;
    bool ok = WeaponFire(w, me, nullptr);
    *mode = oldMode;
    return ok;
}

// A sniper rifle in hand, scope down, no-scope sniper on.
static bool NoScopeSniperInHand(const Settings& A, void* me) {
    if (!A.noScopeSniper || PlayerVehicle()) return false;
    int slot = at<signed char>(me, PED_CUR_WEAPON);
    if (slot < 0 || slot >= WEAPON_SLOTS) return false;
    int type = at<int>((char*)me + PED_WEAPONS + slot * WEAPON_SIZE, WEAP_TYPE);
    return IsScopeWeapon(type) && !ScopedNow();
}

static void AimLockTick(const Settings& A, void* me) {
    static void* locked = nullptr;
    struct Pub { void*& l; ~Pub() { g_aimTarget = l; } } pub{ locked };
    static DWORD releasedUntil = 0;
    DWORD now = GetTickCount();
    // right mouse button - or, with a no-scope sniper, the left one (right would scope in)
    bool aiming = NewMouseState[1] != 0 || (NewMouseState[0] != 0 && NoScopeSniperInHand(A, me)) ||
                  (NewMouseState[0] != 0 && g_bhopping);               // hopping: the fire button alone
    g_aimError = 10.0f;
    if (!A.aimLock || !aiming || PlayerVehicle() || g_menuOpen || *GameMenuActive || !GameFocused()) {
        locked = nullptr; return;
    }
    float mx = *(float*)(NewMouseState + 8), my = *(float*)(NewMouseState + 0xC);
    // player yanks the mouse away (not while hopping: strafing turns are not a let-go)
    if (!g_bhopping && fabsf(mx) + fabsf(my) > 60.0f) { locked = nullptr; releasedUntil = now + 400; return; }
    if (now < releasedUntil) return;

    float src[3], c[3];
    if (!CrosshairRay(me, src, c)) return;

    auto aimPoint = [](void* p, float* out) {
        if (IsHeliTarget(p)) { memcpy(out, &at<float>(p, ENT_POS), 12); return; }   // helicopter: its middle
        EnemyAimPoint(p, out);
    };
    auto visible = [&](const float* a) {
        return LineOfSightClear(src, a, true, false, false, false, false, true, true);   // fences, glass: see through
    };
    auto angleTo = [&](const float* a) {
        float d[3] = { a[0] - src[0], a[1] - src[1], a[2] - src[2] };
        float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (l < 0.5f || l > 250.0f) return 10.0f;
        float dot = (d[0] * c[0] + d[1] * c[1] + d[2] * c[2]) / l;
        return acosf(fmaxf(-1.0f, fminf(1.0f, dot)));
    };

    // keep the current target while it is still a threat and in sight
    float ap[3];
    if (locked) {
        bool ok = IsHeliTarget(locked) || (IsLivePed(locked) && IsHostile(locked, me));
        if (!ok) locked = nullptr;
        else {
            aimPoint(locked, ap);
            if (angleTo(ap) > 9.0f) locked = nullptr;                      // walls don't break it -
            else if (g_bhopping && !A.wallBang && !IsHeliTarget(locked) && !PedReachable(me, src, locked))
                locked = nullptr;                                          // - except silent aim: it must land
        }
    }
    if (!locked) {                                                         // pick the threat nearest the crosshair
        char* pool = (char*)*PedPool; if (!pool) return;
        char* objs = *(char**)(pool + POOL_OBJECTS);
        unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
        int size = *(int*)(pool + POOL_SIZE);
        // all round (360 degrees): the closest threat wins, ones nearer the
        // crosshair a little sooner
        float best = 1e9f;
        for (int i = 0; i < size; i++) {
            if (flags[i] & 0x80) continue;
            void* p = objs + i * PED_POOL_STRIDE;
            if (!IsHostile(p, me)) continue;
            float a[3]; aimPoint(p, a);
            float ang = angleTo(a);
            if (ang > 9.0f) continue;                                      // too near / too far
            float dist = sqrtf((a[0] - src[0]) * (a[0] - src[0]) + (a[1] - src[1]) * (a[1] - src[1]) + (a[2] - src[2]) * (a[2] - src[2]));
            float score;
            if (g_bhopping) {
                // silent aim: the one nearest your crosshair (a little for distance), and only
                // ones a shot can reach unless wall bang is on
                score = ang * 30.0f + dist * 0.05f;
                if (score < best && !A.wallBang && !PedReachable(me, src, p)) continue;
            } else {
                score = dist * (1.0f + ang * 0.5f);
                if (score < best && !visible(a)) score *= 2.5f;            // behind a wall: still a target, seen ones first
            }
            if (score < best) { best = score; locked = p; }
        }
        for (int i = 0; i < HELI_COUNT; i++) {                           // police helicopters too
            void* h = HeliList[i];
            if (!IsHeliTarget(h)) continue;
            float a[3]; aimPoint(h, a);
            float ang = angleTo(a);
            if (ang > 9.0f) continue;
            float dist = sqrtf((a[0] - src[0]) * (a[0] - src[0]) + (a[1] - src[1]) * (a[1] - src[1]) + (a[2] - src[2]) * (a[2] - src[2]));
            float score = g_bhopping ? ang * 30.0f + dist * 0.05f : dist * (1.0f + ang * 0.5f);
            if (score < best) { best = score; locked = h; }
        }
        if (!locked) return;
        aimPoint(locked, ap);
    }

    // Moving targets: bullets land the instant the shot goes off, but the
    // camera turn lands a frame or two later - aim where the target will be
    // by then (its speed relative to the player's).
    {
        static LARGE_INTEGER f0, last;
        LARGE_INTEGER nowc; QueryPerformanceCounter(&nowc);
        if (!f0.QuadPart) QueryPerformanceFrequency(&f0);
        float dt = last.QuadPart ? (float)(nowc.QuadPart - last.QuadPart) / (float)f0.QuadPart : 0.016f;
        last = nowc;
        if (dt > 0.05f) dt = 0.05f;
        float steps = (dt * 2.0f + 0.015f) * 50.0f;                        // game speeds are per 1/50 s
        const float* tv = &at<float>(locked, PHYS_MOVE);
        const float* mv = &at<float>(me, PHYS_MOVE);
        for (int i = 0; i < 2; i++) ap[i] += (tv[i] - mv[i]) * steps;     // sideways only: bumps and
                                                                          // gravity would bob the aim
    }

    // turn the camera so the crosshair lands on the target
    float d[3] = { ap[0] - src[0], ap[1] - src[1], ap[2] - src[2] };
    float dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    for (float& v : d) v /= dl;
    float dh = atan2f(d[1], d[0]) - atan2f(c[1], c[0]);
    while (dh > 3.14159265f) dh -= 6.2831853f;
    while (dh < -3.14159265f) dh += 6.2831853f;
    g_aimError = acosf(fmaxf(-1.0f, fminf(1.0f, c[0] * d[0] + c[1] * d[1] + c[2] * d[2])));
    float pitch = asinf(fmaxf(-1.0f, fminf(1.0f, c[2])));
    float dp = asinf(fmaxf(-1.0f, fminf(1.0f, d[2]))) - pitch;

    // The camera builds its direction from (cos Beta, sin Beta) and sin(Alpha)
    // (CCam 0x479854..0x4798FE) in third-person and first-person weapon views
    // alike, so the view turns the way Beta grows and looks up as Alpha grows.
    // While bunny hopping the camera is never turned (it would turn the run):
    // the target is still picked, and ShotTick lands each shot on it instead.
    if (g_bhopping) return;
    char* cam = ActiveCam();
    const float gain = 0.9f;                                       // nearly all the way, no overshoot
    at<float>(cam, CAM_BETA) += dh * gain;
    at<float>(cam, CAM_ALPHA) += dp * gain;
}

struct Footprint { float halfLen, halfWid, radius, bottom, top; bool avoidWater = true, openSky = false; };   // bottom/top: below/above the centre
static Footprint EntityFootprint(void* e, bool vehicle);

// ---- wall bang / one-shot vehicles --------------------------------------------
// A shot is noticed by the current weapon's "next shot" timer (CWeapon +0x10)
// moving on while the fire button is down. Then the shot's line (camera
// through the crosshair) is followed by the trainer:
//  - wall bang: the first person on that line who is behind a wall, a car,
//    a wreck or anything else gets the weapon's damage as if the bullet went
//    through (people in plain sight are left to the game's own bullet)
//  - one-shot vehicles: the first vehicle on the line - tank, helicopter,
//    armoured truck, anything - is destroyed at once.
static bool RayHitsVehicle(const float* src, const float* dir, float range, void* v, float* tOut) {
    Footprint fp = EntityFootprint(v, true);
    const float* vp = &at<float>(v, ENT_POS);
    const float* ax[3] = { &at<float>(v, ENT_RIGHT), &at<float>(v, ENT_FWD), &at<float>(v, ENT_UP) };
    float pad = 0.3f + 0.008f * sqrtf((vp[0] - src[0]) * (vp[0] - src[0]) + (vp[1] - src[1]) * (vp[1] - src[1]) + (vp[2] - src[2]) * (vp[2] - src[2]));   // more room far away
    float half[3] = { fp.halfWid + pad, fp.halfLen + pad, (fp.bottom + fp.top) * 0.5f + pad };
    float zmid = (fp.top - fp.bottom) * 0.5f;                              // box centre above the origin
    float o[3] = { src[0] - vp[0] - ax[2][0] * zmid, src[1] - vp[1] - ax[2][1] * zmid, src[2] - vp[2] - ax[2][2] * zmid };
    float t0 = 0.0f, t1 = range;
    for (int k = 0; k < 3; k++) {
        float oo = o[0] * ax[k][0] + o[1] * ax[k][1] + o[2] * ax[k][2];
        float dd = dir[0] * ax[k][0] + dir[1] * ax[k][1] + dir[2] * ax[k][2];
        if (fabsf(dd) < 1e-5f) { if (fabsf(oo) > half[k]) return false; continue; }
        float a = (-half[k] - oo) / dd, b = (half[k] - oo) / dd;
        if (a > b) { float t = a; a = b; b = t; }
        t0 = fmaxf(t0, a); t1 = fminf(t1, b);
        if (t0 > t1) return false;
    }
    *tOut = t0;
    return true;
}

static void ShotTick(const Settings& A, void* me) {
    static int lastTimer = 0, lastSlot = -1;
    int slot = at<signed char>(me, PED_CUR_WEAPON);
    if (slot < 0 || slot >= WEAPON_SLOTS) return;
    char* w = (char*)me + PED_WEAPONS + slot * WEAPON_SIZE;
    int timer = at<int>(w, WEAP_TIMER), type = at<int>(w, WEAP_TYPE);
    bool changed = slot == lastSlot && timer != lastTimer;
    lastTimer = timer; lastSlot = slot;
    bool silent = g_bhopping && A.aimLock && g_aimTarget;             // silent aim while hopping
    if (!changed || (!A.wallBang && !A.oneShotVehicles && !silent) || *GameMenuActive) return;
    char* wi = GetWeaponInfo(type);
    if (!wi) return;
    // CWeapon::Fire sets the timer to now + firing rate (CWeaponInfo +8); a
    // reload sets now + reload time instead - only the first is a shot
    int left = timer - (int)*TimeMs, rate = at<int>(wi, WI_FIRE_RATE);
    if (left > rate + 5 || left < rate - 80) return;
    int fire = at<int>(wi, WI_FIRETYPE);                                   // 1 instant hit, 2 projectile
    if (fire != 1 && fire != 2) return;
    float range = fire == 1 ? fminf(fmaxf(at<float>(wi, WI_RANGE), 30.0f), 300.0f) : 200.0f;
    float damage = (float)at<int>(wi, WI_DAMAGE);
    float src[3], dir[3];
    if (!CrosshairRay(me, src, dir)) return;
    void* myVeh = PlayerVehicle();

    // Silent aim (bunny hopping with the aim lock on): the shot lands on the
    // locked target wherever the camera points - in sight (fences and glass
    // don't count), or anywhere with wall bang.
    if (silent) {
        void* t = g_aimTarget;
        if (IsHeliTarget(t)) {
            if (A.oneShotVehicles || fire == 2) ShootDownHeli(t);
        } else if (IsLivePed(t) && at<float>(t, PED_HEALTH) > 0.0f) {
            float a[3]; EnemyAimPoint(t, a);
            (void)a;
            if (A.wallBang || PedReachable(me, src, t))
                InflictDamage(t, me, type, fire == 2 ? 1000.0f : fmaxf(damage, 25.0f), 0, 0);
        }
        if (!A.wallBang && !A.oneShotVehicles) return;
    }

    if (A.oneShotVehicles) {
        char* pool = (char*)*VehiclePool;
        if (pool) {
            char* objs = *(char**)(pool + POOL_OBJECTS);
            unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
            int size = *(int*)(pool + POOL_SIZE);
            void* hit = nullptr; float best = 400.0f;                    // any weapon: helicopters fly high and far
            for (int i = 0; i < size; i++) {
                if (flags[i] & 0x80) continue;
                void* v = objs + i * VEHICLE_POOL_STRIDE;
                if (v == myVeh) continue;
                if (at<int>(v, VEH_TYPE) == 3 ? !IsHeliTarget(v) : at<float>(v, VEH_HEALTH) <= 0.0f) continue;
                float t;
                if (RayHitsVehicle(src, dir, best, v, &t) && t < best) { best = t; hit = v; }
            }
            if (hit) {
                float e[3] = { src[0] + dir[0] * best, src[1] + dir[1] * best, src[2] + dir[2] * best };
                bool reach = A.wallBang || LineOfSightClear(src, e, true, false, false, false, false, true, true);
                if (reach && at<int>(hit, VEH_TYPE) == 3) ShootDownHeli(hit);   // police helicopter
                else if (reach) {
                    at<unsigned char>(hit, VEH_FLAGS_1FB) |= 0x40;           // can be damaged
                    at<unsigned char>(hit, PHYS_FLAGS1) &= ~0x02;            // not explosion proof
                    at<unsigned char>(hit, PHYS_FLAGS2) &= ~0x06;            // not bullet / fire proof
                    at<float>(hit, VEH_HEALTH) = 0.0f;
                    VehicleBlowUp(hit, me);
                }
            }
        }
    }

    if (A.wallBang && fire == 1) {
        char* pool = (char*)*PedPool;
        if (!pool) return;
        char* objs = *(char**)(pool + POOL_OBJECTS);
        unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
        int size = *(int*)(pool + POOL_SIZE);
        void* hit = nullptr; float best = range;
        for (int i = 0; i < size; i++) {
            if (flags[i] & 0x80) continue;
            void* p = objs + i * PED_POOL_STRIDE;
            if (p == me) continue;
            int st = at<int>(p, PED_STATE);
            if (st == PED_DIE || st == PED_DEAD || at<float>(p, PED_HEALTH) <= 0.0f) continue;
            if (myVeh && at<void*>(p, PED_MY_VEHICLE) == myVeh && at<unsigned char>(p, PED_IN_VEHICLE)) continue;
            float a[3]; EnemyAimPoint(p, a);
            float d[3] = { a[0] - src[0], a[1] - src[1], a[2] - src[2] };
            float t = d[0] * dir[0] + d[1] * dir[1] + d[2] * dir[2];
            if (t < 1.0f || t > best) continue;
            float px = d[0] - dir[0] * t, py = d[1] - dir[1] * t, pz = d[2] - dir[2] * t;
            float off = sqrtf(px * px + py * py + pz * pz);
            if (off > 0.55f + t * 0.004f) continue;                          // on the line (wider far away)
            best = t; hit = p;
        }
        if (hit) {
            float a[3]; EnemyAimPoint(hit, a);
            // only when something is in the way - in plain sight the game's own bullet does it
            if (!LineOfSightClear(src, a, true, true, false, true, true, false, false))
                InflictDamage(hit, me, type, fmaxf(damage, 25.0f), 0, 0);
        }
    }
}

// ---- genocide ------------------------------------------------------------------
// While on, every ped around dies and every vehicle around blows up - the
// player, the player's vehicle and anyone riding in it excepted. New ones
// that stream in go the same way. Vehicles go a few at a time so the
// explosions don't stall the game.
static void GenocideTick(const Settings& A, void* me) {
    static DWORD next = 0;
    if (!A.genocide || *GameMenuActive) return;
    DWORD now = GetTickCount();
    if (now < next) return;
    next = now + 120;
    void* myVeh = PlayerVehicle();
    if (char* pool = (char*)*PedPool) {
        char* objs = *(char**)(pool + POOL_OBJECTS);
        unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
        int size = *(int*)(pool + POOL_SIZE);
        for (int i = 0; i < size; i++) {
            if (flags[i] & 0x80) continue;
            void* p = objs + i * PED_POOL_STRIDE;
            if (p == me) continue;
            int st = at<int>(p, PED_STATE);
            if (st == PED_DIE || st == PED_DEAD || at<float>(p, PED_HEALTH) <= 0.0f) continue;
            if (myVeh && at<unsigned char>(p, PED_IN_VEHICLE) && at<void*>(p, PED_MY_VEHICLE) == myVeh) continue;
            InflictDamage(p, me, 45 /* UNIDENTIFIED */, 1000.0f, 0, 0);
            at<float>(p, PED_ARMOUR) = 0.0f;
            at<float>(p, PED_HEALTH) = 0.0f;
        }
    }
    if (char* pool = (char*)*VehiclePool) {
        char* objs = *(char**)(pool + POOL_OBJECTS);
        unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
        int size = *(int*)(pool + POOL_SIZE);
        int budget = 4;
        for (int i = 0; i < size && budget > 0; i++) {
            if (flags[i] & 0x80) continue;
            void* v = objs + i * VEHICLE_POOL_STRIDE;
            if (v == myVeh) continue;
            if (at<int>(v, VEH_TYPE) == 3) { if (IsHeliTarget(v)) { ShootDownHeli(v); budget--; } continue; }
            if (at<float>(v, VEH_HEALTH) <= 0.0f || (at<unsigned char>(v, ENT_STATUS) >> 3) == 5) continue;
            at<unsigned char>(v, VEH_FLAGS_1FB) |= 0x40;
            at<unsigned char>(v, PHYS_FLAGS1) &= ~0x02;
            at<float>(v, VEH_HEALTH) = 0.0f;
            VehicleBlowUp(v, me);
            budget--;
        }
    }
}

// ---- rapid fire ---------------------------------------------------------------
// After every shot the weapon waits for its timer (CWeapon +0x10, now + the
// weapon's firing rate). While firing, the wait is cut to a few milliseconds,
// so guns shoot as fast as the game will run them. Reloads keep their normal
// time unless "no reload" is on too.
static void RapidFireTick(const Settings& A, void* me) {
    static DWORD nextShot = 0;
    if (!A.rapidFire || *GameMenuActive || g_menuOpen || !GameFocused() || PlayerVehicle()) return;
    if (!NewMouseState[0]) return;                                         // fire button held
    int slot = at<signed char>(me, PED_CUR_WEAPON);
    if (slot < 0 || slot >= WEAPON_SLOTS) return;
    char* w = (char*)me + PED_WEAPONS + slot * WEAPON_SIZE;
    if (at<int>(w, WEAP_STATE) == WEAPSTATE_RELOADING || at<int>(w, WEAP_CLIP) <= 0) return;
    int type = at<int>(w, WEAP_TYPE);
    char* wi = GetWeaponInfo(type);
    if (!wi) return;
    int fire = at<int>(wi, WI_FIRETYPE);
    if (fire != 1 && fire != 2 && fire != 3) return;                       // guns, launchers, flamethrower
    if (IsScopeWeapon(type) && A.noScopeSniper && !ScopedNow()) return;  // hip shots are the no-scope feature's
    DWORD now = GetTickCount();
    if (now < nextShot) return;
    nextShot = now + 45;                                                   // ~22 shots a second, any gun
    // The game's own shot - one of these alone does not need the weapon's
    // animation or a fresh click, so holding the button keeps it going.
    FireAnyView(w, me);
    if (g_flyingOnFoot) PushProjectilesOut(me);
}

// Rocket launcher without the sight: the game's own rocket (FireProjectile -
// smoke trail, explosion), then pointed down the crosshair, or at the aim
// lock's target, by turning the new rocket object and its speed.
static bool FireRocketAtCrosshair(char* w, void* me, float* source, void* target) {
    // For the player the game only launches with the camera in a scope / sight
    // view (FireProjectile 0x5CCFE4 checks the mode) - so it is in rocket-sight
    // mode for just this call.
    short* mode = &at<short>(ActiveCam(), CAM_MODE);
    short oldMode = *mode;
    *mode = 8;
    bool ok = FireProjectile(w, me, source, *ShotVolume);
    *mode = oldMode;
    if (!ok) return false;
    float src[3], dir[3];
    if (!CrosshairRay(me, src, dir)) return true;
    if (target) {
        float a[3];
        if (IsHeliTarget(target)) memcpy(a, &at<float>(target, ENT_POS), 12);
        else EnemyAimPoint(target, a);
        float d[3] = { a[0] - source[0], a[1] - source[1], a[2] - source[2] };
        float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (l > 0.5f) for (int i = 0; i < 3; i++) dir[i] = d[i] / l;
    } else {
        // from the launcher to where the crosshair line meets something
        float farPt[3] = { src[0] + dir[0] * 150.0f, src[1] + dir[1] * 150.0f, src[2] + dir[2] * 150.0f };
        float d[3] = { farPt[0] - source[0], farPt[1] - source[1], farPt[2] - source[2] };
        float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (l > 0.5f) for (int i = 0; i < 3; i++) dir[i] = d[i] / l;
    }
    // the rocket that just appeared next to the player
    char* pool = (char*)*ObjectPool;
    if (!pool) return true;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    void* best = nullptr; float bestD = 6.0f;
    for (int i = 0; i < size; i++) {
        if (flags[i] & 0x80) continue;
        void* o = objs + i * OBJECT_POOL_STRIDE;
        if (at<short>(o, ENT_MODEL) != MODEL_MISSILE) continue;
        const float* op = &at<float>(o, ENT_POS);
        float dd = sqrtf((op[0] - source[0]) * (op[0] - source[0]) + (op[1] - source[1]) * (op[1] - source[1]) +
                         (op[2] - source[2]) * (op[2] - source[2]));
        if (dd < bestD) { bestD = dd; best = o; }
    }
    if (!best) return true;
    float* mv = &at<float>(best, PHYS_MOVE);
    float speed = sqrtf(mv[0] * mv[0] + mv[1] * mv[1] + mv[2] * mv[2]);
    if (speed < 0.5f) speed = 1.5f;
    for (int i = 0; i < 3; i++) mv[i] = dir[i] * speed;
    // point it the same way (forward = dir, level right, up)
    float r[3] = { dir[1], -dir[0], 0 };
    float rl = sqrtf(r[0] * r[0] + r[1] * r[1]);
    if (rl < 0.001f) { r[0] = 1; r[1] = 0; rl = 1; }
    r[0] /= rl; r[1] /= rl;
    float u[3] = { r[1] * dir[2] - 0 * dir[1], 0 * dir[0] - r[0] * dir[2], r[0] * dir[1] - r[1] * dir[0] };
    memcpy(&at<float>(best, ENT_RIGHT), r, 12);
    memcpy(&at<float>(best, ENT_FWD), dir, 12);
    memcpy(&at<float>(best, ENT_UP), u, 12);
    return true;
}

// ---- no-scope sniper -----------------------------------------------------------
// Sniper rifle without the scope: a left click fires the game's ordinary
// bullet (FireInstantHit - tracer, impact, blood) down the third-person
// crosshair, with the game's own gunshot sound, ammo and fire-rate timer.
// With aim lock on, the click first lets the crosshair swing onto the target
// (a few frames at most), then fires - and the hit is made sure to kill.
static void NoScopeSniperTick(const Settings& A, void* me) {
    static bool wasDown = false, pending = false;
    static DWORD pendingSince = 0;
    bool down = NewMouseState[0] != 0;
    bool click = down && !wasDown;
    wasDown = down;
    if (*GameMenuActive || g_menuOpen || !GameFocused() || !NoScopeSniperInHand(A, me)) { pending = false; return; }
    int slot = at<signed char>(me, PED_CUR_WEAPON);
    char* w = (char*)me + PED_WEAPONS + slot * WEAPON_SIZE;
    int type = at<int>(w, WEAP_TYPE);
    char* wi = GetWeaponInfo(type);
    if (!wi) return;
    DWORD now = GetTickCount();
    static DWORD nextAuto = 0;
    if (A.sniperAuto && down && !pending && now >= nextAuto) click = true;   // held: keep firing
    if (click) {
        nextAuto = now + 90;
        if (at<int>(w, WEAP_STATE) == WEAPSTATE_RELOADING) return;
        if (at<int>(w, WEAP_CLIP) <= 0 && at<int>(w, WEAP_TOTAL) <= 0) return;
        if (!A.rapidFire && !A.sniperAuto && (int)(at<int>(w, WEAP_TIMER) - (int)*TimeMs) > 0) return;   // between shots
        pending = true; pendingSince = now;
    }
    if (!pending) return;
    bool onTarget = !A.aimLock || !g_aimTarget || g_aimError < 0.02f;
    if (!onTarget && now - pendingSince < 160) return;                     // let the lock swing on first
    pending = false;

    const float* mp = &at<float>(me, ENT_POS);
    float source[3] = { mp[0], mp[1], mp[2] + 0.6f };
    void* target = A.aimLock ? g_aimTarget : nullptr;
    bool rocket = type == WEAPON_ROCKET_LAUNCHER;
    if (rocket) {
        if (!FireRocketAtCrosshair(w, me, source, target)) return;          // nothing launched: no ammo used
        if (g_flyingOnFoot) PushProjectilesOut(me);
    }
    else FireInstantHit(w, me, source);
    PlayOneShot(DMAudio, at<int>(me, ENT_AUDIO_ID), SOUND_WEAPON_SHOT_FIRED, *ShotVolume);
    if (at<int>(w, WEAP_CLIP) > 0) at<int>(w, WEAP_CLIP)--;
    if (at<int>(w, WEAP_TOTAL) > 0) at<int>(w, WEAP_TOTAL)--;
    at<int>(w, WEAP_TIMER) = (int)*TimeMs + at<int>(wi, WI_FIRE_RATE);
    if (rocket) return;                                                    // the rocket does its own damage

    // one click, one kill: whoever the shot was for (the lock's target, or the
    // first person on the crosshair line) - if in sight and still standing
    float src[3], dir[3];
    if (!target && CrosshairRay(me, src, dir)) {
        char* pool = (char*)*PedPool;
        if (pool) {
            char* objs = *(char**)(pool + POOL_OBJECTS);
            unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
            int size = *(int*)(pool + POOL_SIZE);
            float best = 300.0f;
            for (int i = 0; i < size; i++) {
                if (flags[i] & 0x80) continue;
                void* p = objs + i * PED_POOL_STRIDE;
                if (p == me) continue;
                int st = at<int>(p, PED_STATE);
                if (st == PED_DIE || st == PED_DEAD || at<float>(p, PED_HEALTH) <= 0.0f) continue;
                float a[3]; EnemyAimPoint(p, a);
                float d[3] = { a[0] - src[0], a[1] - src[1], a[2] - src[2] };
                float t = d[0] * dir[0] + d[1] * dir[1] + d[2] * dir[2];
                if (t < 1.0f || t > best) continue;
                float px = d[0] - dir[0] * t, py = d[1] - dir[1] * t, pz = d[2] - dir[2] * t;
                if (sqrtf(px * px + py * py + pz * pz) > 0.6f + t * 0.004f) continue;
                best = t; target = p;
            }
        }
    }
    if (target && IsLivePed(target) && at<float>(target, PED_HEALTH) > 0.0f) {
        if (!CrosshairRay(me, src, dir)) return;
        float a[3]; EnemyAimPoint(target, a);
        if (A.wallBang || LineOfSightClear(src, a, true, true, false, true, true, true, true))
            InflictDamage(target, me, type, fmaxf((float)at<int>(wi, WI_DAMAGE), 250.0f), 0, 0);
    }
}

// ---- sniper full auto (scoped) ------------------------------------------------
// In the scope the game wants a fresh click per shot; with this on, holding
// the button fires the game's own sniper shot (CWeapon::Fire -> FireSniper,
// sound and all) about 11 times a second.
static void SniperAutoTick(const Settings& A, void* me) {
    static DWORD next = 0;
    if (!A.sniperAuto || !NewMouseState[0] || *GameMenuActive || g_menuOpen || !GameFocused() || PlayerVehicle()) return;
    int slot = at<signed char>(me, PED_CUR_WEAPON);
    if (slot < 0 || slot >= WEAPON_SLOTS) return;
    char* w = (char*)me + PED_WEAPONS + slot * WEAPON_SIZE;
    int type = at<int>(w, WEAP_TYPE);
    if (!IsScopeWeapon(type) || !ScopedNow()) return;                     // scope down: the no-scope part handles it
    if (at<int>(w, WEAP_STATE) == WEAPSTATE_RELOADING || at<int>(w, WEAP_CLIP) <= 0) return;
    DWORD now = GetTickCount();
    if (now < next) return;
    next = now + 90;
    WeaponFire(w, me, nullptr);
}

// ---- super jump / super run -----------------------------------------------------
// Super jump: the moment a jump starts (CPed state -> PED_JUMP), the upward
// speed is multiplied, plus a little more forward push. Super run: every
// frame on foot, the distance the player actually moved is stretched - so
// walking, running and sprinting all get faster - unless a wall is in the way.
static void SuperMoveTick(const Settings& A, void* me) {
    static int lastState = -1;
    static float lastPos[3];
    static bool havePos = false;
    if (PlayerVehicle() || *GameMenuActive) { havePos = false; lastState = -1; return; }
    int st = at<int>(me, PED_STATE);
    float* mv = &at<float>(me, PHYS_MOVE);
    if (A.superJump && st == PED_JUMP && lastState != PED_JUMP) {
        float up = fmaxf(mv[2], 0.12f);
        mv[2] = up * A.superJumpPower;
        mv[0] *= 1.0f + (A.superJumpPower - 1.0f) * 0.35f;
        mv[1] *= 1.0f + (A.superJumpPower - 1.0f) * 0.35f;
    }
    lastState = st;

    float* pos = &at<float>(me, ENT_POS);
    if (A.superRun && havePos && (at<unsigned char>(me, PED_FLAGS_14C) & 1) && st != PED_JUMP && st != PED_FALL) {
        float dx = pos[0] - lastPos[0], dy = pos[1] - lastPos[1];
        float d = sqrtf(dx * dx + dy * dy);
        if (d > 0.005f && d < 1.0f) {                                      // really moving, not a teleport
            float k = A.superRunSpeed - 1.0f;
            float to[3] = { pos[0] + dx * k, pos[1] + dy * k, pos[2] };
            float a[3] = { pos[0], pos[1], pos[2] + 0.2f }, b[3] = { to[0] + dx / d * 0.4f, to[1] + dy / d * 0.4f, pos[2] + 0.2f };
            if (LineOfSightClear(a, b, true, true, false, true, false, false, false)) {
                bool f = false;
                float gz = FindGroundZ3D(to[0], to[1], pos[2] + 1.0f, &f);
                if (f && fabsf(gz + 1.0f - pos[2]) < 1.2f) {                 // same level, no ledge
                    EntityRemove(me);
                    pos[0] = to[0]; pos[1] = to[1];
                    EntityAdd(me);
                }
            }
        }
    }
    memcpy(lastPos, pos, 12);
    havePos = true;
}

// ---- auto bunny hop / no landing roll ------------------------------------------
// Fade an animation out at once if the player has it.
static bool KillAnim(void* ped, int animId) {
    void* clump = at<void*>(ped, ENT_CLUMP);
    if (!clump) return false;
    char* a = GetAnimAssoc(clump, animId);
    if (!a) return false;
    at<float>(a, ASSOC_BLEND) = 0.0f;
    at<float>(a, ASSOC_DELTA) = -100.0f;
    at<unsigned short>(a, ASSOC_FLAGS) |= ASSOCF_DELETEFADEDOUT;
    return true;
}

// Landing from a fall: no forward roll (FALL_collapse), crouch (FALL_land) or
// get-up - the player is straight back on their feet and in control.
static bool CancelLandingAnims(void* ped, bool alsoJumpLand) {
    bool any = false;
    any |= KillAnim(ped, ANIM_FALL_COLLAPSE);
    any |= KillAnim(ped, ANIM_FALL_LAND);
    for (int a = ANIM_GETUP1; a <= ANIM_GETUP_FRONT; a++) any |= KillAnim(ped, a);
    if (alsoJumpLand) any |= KillAnim(ped, ANIM_JUMP_LAND);
    int st = at<int>(ped, PED_STATE);
    if (any && (st == PED_FALL || st == PED_GETUP)) at<int>(ped, PED_STATE) = PED_IDLE;
    return any;
}

// Bunny hop, Source-engine style. Hold jump and you hop again the moment you
// land. While hopping the trainer owns the sideways movement, smoothly, every
// frame: a velocity in m/s that the game's own walking doesn't touch (its
// sideways speed is zeroed so the two don't fight; gravity and height stay
// the game's). In the air it follows Quake/Source air acceleration - W/A/S/D
// relative to the camera, a small air speed cap, so turning the mouse while
// holding A or D curves you and gains speed (air strafing). Each landing hop
// multiplies the speed by the gain - any direction, only while moving forward
// (AFH) or only while moving backward relative to the camera (ABH) - up to
// the top speed. Walls stop you; let go of jump and your speed carries into
// the landing.
static bool KeyHeld(int vk);

// A CS:GO-style hop: straight off the ground the same frame - no take-off
// crouch (JUMP_launch), no landing (JUMP_land / FALL_land) - into the in-air
// pose (JUMP_glide), with the push up given directly for the chosen hop
// height (more with super jump).
static void InstantHop(const Settings& A, void* me) {
    KillAnim(me, ANIM_JUMP_LAUNCH);
    KillAnim(me, ANIM_JUMP_LAND);
    KillAnim(me, ANIM_FALL_LAND);
    KillAnim(me, ANIM_FALL_COLLAPSE);
    if (void* clump = at<void*>(me, ENT_CLUMP)) {
        char* a = GetAnimAssoc(clump, ANIM_JUMP_GLIDE);
        if (!a) BlendAnimation(clump, 0, ANIM_JUMP_GLIDE, 16.0f);
    }
    at<int>(me, PED_STATE) = PED_JUMP;
    float* mv = &at<float>(me, PHYS_MOVE);
    // speed up for the chosen height: v = sqrt(2 g h), gravity 0.008 per 1/50 s step
    float h = A.bhopHeight * (A.superJump ? A.superJumpPower : 1.0f);
    mv[2] = sqrtf(2.0f * 0.008f * h);
    at<unsigned char>(me, PED_FLAGS_14C) &= ~0x03;                         // off the ground
    EntityRemove(me);
    at<float>(me, ENT_POS + 8) += 0.05f;
    EntityAdd(me);
}

// Back to normal on the ground: the in-air pose and any jump / landing
// animation go, the standing animation comes in and the ped is idle, so the
// game's own walking / running takes over again at once.
static void EndHop(void* me) {
    KillAnim(me, ANIM_JUMP_GLIDE);
    KillAnim(me, ANIM_JUMP_LAUNCH);
    KillAnim(me, ANIM_JUMP_LAND);
    KillAnim(me, ANIM_FALL_LAND);
    KillAnim(me, ANIM_FALL_COLLAPSE);
    if (void* clump = at<void*>(me, ENT_CLUMP))
        if (!GetAnimAssoc(clump, ANIM_IDLE_STANCE)) BlendAnimation(clump, 0, ANIM_IDLE_STANCE, 16.0f);
    int st = at<int>(me, PED_STATE);
    if (st == PED_JUMP || st == PED_FALL || st == PED_GETUP) at<int>(me, PED_STATE) = PED_IDLE;
}

// What a short line hits: point and surface normal (CWorld::ProcessLineOfSight,
// buildings and objects; fences / glass count).
static bool RayHit(const float* a, const float* b, float* point, float* normal) {
    unsigned char cp[64] = {};
    void* ent = nullptr;
    if (!ProcessLineOfSight(a, b, cp, &ent, true, false, false, true, false, false, true, false)) return false;
    memcpy(point, cp, 12);
    memcpy(normal, cp + COLPOINT_NORMAL, 12);
    return true;
}

// Source ClipVelocity: take away the part of v going into the surface.
static void ClipVel(float* v, const float* n, int dims) {
    float back = 0;
    for (int i = 0; i < dims; i++) back += v[i] * n[i];
    if (back < 0.0f) for (int i = 0; i < dims; i++) v[i] -= n[i] * back;
}

// Surf: a ramp steeper than about 44 degrees (normal z 0.05..0.72) right
// against the player - looked for straight down and diagonally down to the
// sides, front and back of the way they're going.
static bool FindSurfRamp(void* me, const float* vel, float* hit, float* n) {
    const float* p = &at<float>(me, ENT_POS);
    float o[3] = { p[0], p[1], p[2] - 0.4f };
    float hx = vel[0], hy = vel[1], hl = sqrtf(hx * hx + hy * hy);
    if (hl > 0.1f) { hx /= hl; hy /= hl; } else { hx = CameraFront[0]; hy = CameraFront[1]; hl = sqrtf(hx * hx + hy * hy); if (hl > 0.01f) { hx /= hl; hy /= hl; } else { hx = 0; hy = 1; } }
    const float dirs[5][3] = { { 0, 0, -1.0f }, { hy * 0.75f, -hx * 0.75f, -0.75f }, { -hy * 0.75f, hx * 0.75f, -0.75f },
                               { hx * 0.75f, hy * 0.75f, -0.75f }, { -hx * 0.75f, -hy * 0.75f, -0.75f } };
    for (auto& d : dirs) {
        float e[3] = { o[0] + d[0], o[1] + d[1], o[2] + d[2] };
        float hp[3], hn[3];
        if (!RayHit(o, e, hp, hn)) continue;
        if (hn[2] > 0.05f && hn[2] < 0.72f) { memcpy(hit, hp, 12); memcpy(n, hn, 12); return true; }
    }
    return false;
}

static void BhopTick(const Settings& A, void* me) {
    static float vel[3] = { 0, 0, 0 };                                    // m/s, the trainer's own movement
    static bool chain = false, wasStanding = true, surfing = false;
    static LARGE_INTEGER freq, last;
    LARGE_INTEGER nowc; QueryPerformanceCounter(&nowc);
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    float dt = last.QuadPart ? (float)(nowc.QuadPart - last.QuadPart) / (float)freq.QuadPart : 0.0f;
    last = nowc;
    if (dt > 0.05f) dt = 0.05f;

    struct Pub { bool& c; ~Pub() { g_bhopping = c; } } pub{ chain };
    if (PlayerVehicle() || *GameMenuActive || g_menuOpen || !GameFocused()) { chain = surfing = false; return; }
    bool standing = (at<unsigned char>(me, PED_FLAGS_14C) & 1) != 0;
    int st = at<int>(me, PED_STATE);
    float* mv = &at<float>(me, PHYS_MOVE);
    bool held = at<short>(Pad0, PAD_JUMP) != 0;

    if (A.noLandingRoll || (A.autoBhop && chain)) {
        if (standing && !wasStanding && !surfing) CancelLandingAnims(me, A.autoBhop && held);
        else if (A.noLandingRoll && (st == PED_FALL || st == PED_GETUP)) CancelLandingAnims(me, false);
    }
    if (!A.autoBhop || st == PED_DIE || st == PED_DEAD) {
        if ((chain || surfing) && st != PED_DIE && st != PED_DEAD) EndHop(me);
        chain = surfing = false; wasStanding = standing; return;
    }

    // "Forward" (AFH / ABH, air strafing) is where the camera looks; the aim
    // lock never turns the camera while hopping.
    float fx = CameraFront[0], fy = CameraFront[1];
    {
        float cl = sqrtf(fx * fx + fy * fy);
        if (cl > 0.01f) { fx /= cl; fy /= cl; } else { fx = 0; fy = 1; }
    }
    float rx = fy, ry = -fx;

    // ---- surf: on a steep ramp the trainer moves the player in 3D ----
    float rampHit[3], rampN[3];
    // Only start surfing from the air; stop as soon as there is flat,
    // walkable ground right under the feet (a steep bank beside the path is
    // not a ramp you're on), or when it has barely moved for half a second.
    static DWORD slowSince = 0;
    bool onRamp = false;
    if (A.bhopSurf && (surfing || !standing)) {
        onRamp = FindSurfRamp(me, (chain || surfing) ? vel : mv, rampHit, rampN);
        if (onRamp) {
            const float* pp = &at<float>(me, ENT_POS);
            float a[3] = { pp[0], pp[1], pp[2] }, b[3] = { pp[0], pp[1], pp[2] - 1.25f };
            float gp[3], gn[3];
            if (RayHit(a, b, gp, gn) && gn[2] >= 0.72f) onRamp = false;     // standing on flat ground
        }
        if (onRamp && surfing) {
            float sp = sqrtf(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2]);
            DWORD nowT = GetTickCount();
            if (sp > 1.0f) slowSince = 0;
            else if (!slowSince) slowSince = nowT;
            else if (nowT - slowSince > 500) onRamp = false;                 // stuck against it: let go
        } else slowSince = 0;
    }
    if (onRamp && !surfing) {                                             // just touched a ramp: take the speed
        if (!chain) { vel[0] = mv[0] * 50.0f; vel[1] = mv[1] * 50.0f; }
        vel[2] = mv[2] * 50.0f;
    }
    if (!onRamp && surfing) {                                             // came off: the game carries on
        mv[0] = vel[0] / 50.0f; mv[1] = vel[1] / 50.0f; mv[2] = vel[2] / 50.0f;
        if (!chain || !held) {                                            // not hopping on: back to normal
            EndHop(me);
            chain = false;
        }
    }
    surfing = onRamp;

    // About to touch down? Hop now, before a frame on the ground (not on a ramp).
    bool landing = !surfing && standing && (st != PED_JUMP || !wasStanding);
    if (!landing && !surfing && chain && held && mv[2] < 0.0f) {
        const float* pp = &at<float>(me, ENT_POS);
        bool f = false;
        float g = FindGroundZ3D(pp[0], pp[1], pp[2] + 0.3f, &f);
        float next = pp[2] - 1.0f + mv[2] * dt * 50.0f;                    // feet next frame
        if (f && next - g < 0.06f) landing = true;
    }
    // hop: on the ground with jump held
    if (held && landing) {
        if (!chain) {                                                       // first hop: take the running speed
            vel[0] = mv[0] * 50.0f; vel[1] = mv[1] * 50.0f;
        } else {
            float sp = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
            float along = sp > 0.01f ? (vel[0] * fx + vel[1] * fy) / sp : 1.0f;   // +1 forward, -1 backward
            bool gain = A.bhopMode == 0 || (A.bhopMode == 1 && along >= 0.0f) || (A.bhopMode == 2 && along < 0.0f);
            float k = gain ? A.bhopGain : 1.0f;
            vel[0] *= k; vel[1] *= k;
            if (sp < 1.0f && A.bhopGain > 1.0f) {                           // standing hop: a little push to start
                float px = A.bhopMode == 2 ? -fx : fx, py = A.bhopMode == 2 ? -fy : fy;
                vel[0] += px * 2.0f; vel[1] += py * 2.0f;
            }
        }
        float sp = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
        if (sp > A.bhopMax) { vel[0] *= A.bhopMax / sp; vel[1] *= A.bhopMax / sp; }
        chain = true;
        InstantHop(A, me);
        standing = false;
        st = PED_JUMP;
    }
    if (chain && !held && standing && !surfing) {                           // let go and landed: back to normal
        chain = false;
        EndHop(me);
        mv[0] = vel[0] / 50.0f * 0.5f; mv[1] = vel[1] / 50.0f * 0.5f;     // a short slide, then the game's own run
    }

    // Hold the jump pose the whole chain / surf: at speed the game blends its
    // walk / run animations back in, which wiped the pose.
    if (chain || surfing) {
        void* clump = at<void*>(me, ENT_CLUMP);
        if (clump) {
            char* g = GetAnimAssoc(clump, ANIM_JUMP_GLIDE);
            if (!g) g = (char*)BlendAnimation(clump, 0, ANIM_JUMP_GLIDE, 1000.0f);
            if (g) { at<float>(g, ASSOC_BLEND) = 1.0f; at<float>(g, ASSOC_DELTA) = 0.0f; }
            for (int a = 0; a <= 2; a++) KillAnim(me, a);                 // walk / run / sprint
            KillAnim(me, ANIM_IDLE_STANCE);
            KillAnim(me, ANIM_JUMP_LAND);
            KillAnim(me, ANIM_FALL_LAND);
            KillAnim(me, ANIM_FALL_COLLAPSE);
        }
        if (at<int>(me, PED_STATE) != PED_JUMP && (!standing || surfing)) at<int>(me, PED_STATE) = PED_JUMP;
    }

    // The game doesn't shoot while it thinks you're mid-jump, which is most
    // of a hop chain - so with the fire button held the trainer fires the
    // gun itself at the weapon's normal rate (rapid fire does its own).
    if ((chain || surfing) && NewMouseState[0] && !A.rapidFire) {
        int slot = at<signed char>(me, PED_CUR_WEAPON);
        if (slot >= 0 && slot < WEAPON_SLOTS) {
            char* w = (char*)me + PED_WEAPONS + slot * WEAPON_SIZE;
            char* wi = GetWeaponInfo(at<int>(w, WEAP_TYPE));
            int ft = wi ? at<int>(wi, WI_FIRETYPE) : 0;
            bool ownFeature = IsScopeWeapon(at<int>(w, WEAP_TYPE)) && (A.noScopeSniper || A.sniperAuto);
            if (wi && (ft == 1 || ft == 2 || ft == 3) && !ownFeature &&
                at<int>(w, WEAP_STATE) != WEAPSTATE_RELOADING &&
                (int)(at<int>(w, WEAP_TIMER) - (int)*TimeMs) <= 0)
                FireAnyView(w, me);
        }
    }

    if ((chain || surfing) && dt > 0.0f) {
        // Air strafing, CS:GO numbers: W/S/A/D give a wish direction relative
        // to the camera; sv_airaccelerate 12, wish speed 250 u/s (6.35 m/s)
        // capped at 30 u/s (0.76 m/s) in the air.
        float wf = (KeyHeld('W') ? 1.0f : 0.0f) - (KeyHeld('S') ? 1.0f : 0.0f);
        float ws = (KeyHeld('D') ? 1.0f : 0.0f) - (KeyHeld('A') ? 1.0f : 0.0f);
        float wx = fx * wf + rx * ws, wy = fy * wf + ry * ws;
        float wl = sqrtf(wx * wx + wy * wy);
        if (wl > 0.01f && (!standing || surfing)) {
            wx /= wl; wy /= wl;
            const float wishCap = 0.762f, airAccel = 12.0f * 6.35f;
            float cur = vel[0] * wx + vel[1] * wy;
            float add = wishCap - cur;
            if (add > 0.0f) {
                float acc = fminf(airAccel * dt, add);
                vel[0] += wx * acc; vel[1] += wy * acc;
            }
        }
        float* pos = &at<float>(me, ENT_POS);
        if (surfing) {
            // gravity (Vice City's is CS:GO's: ~20 m/s2), then slide along the
            // ramp with no friction - the part of the speed into the ramp is
            // taken away (strafing into it is what keeps you on it)
            vel[2] -= 20.0f * dt;
            ClipVel(vel, rampN, 3);
            float sp = sqrtf(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2]);
            if (sp > A.bhopMax) for (int i = 0; i < 3; i++) vel[i] *= A.bhopMax / sp;
            mv[0] = mv[1] = mv[2] = 0.0f;
            float np[3] = { pos[0] + vel[0] * dt, pos[1] + vel[1] * dt, pos[2] + vel[2] * dt };
            // stay on the ramp's surface: the body (0.45 m from the centre
            // towards the ramp) may not sink into it
            float bx = np[0] - rampN[0] * 0.45f, by = np[1] - rampN[1] * 0.45f, bz = np[2] - rampN[2] * 0.45f;
            float dist = (bx - rampHit[0]) * rampN[0] + (by - rampHit[1]) * rampN[1] + (bz - rampHit[2]) * rampN[2];
            if (dist < 0.0f) for (int i = 0; i < 3; i++) np[i] -= rampN[i] * dist;
            float a[3] = { pos[0], pos[1], pos[2] + 0.2f }, b[3] = { np[0], np[1], np[2] + 0.2f };
            if (LineOfSightClear(a, b, true, true, false, true, false, true, false)) {
                EntityRemove(me);
                memcpy(pos, np, 12);
                EntityAdd(me);
            }
        } else {
            float sp = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
            if (sp > A.bhopMax) { vel[0] *= A.bhopMax / sp; vel[1] *= A.bhopMax / sp; }
            // sideways only - the game keeps the height (jump arc, gravity)
            mv[0] = 0.0f; mv[1] = 0.0f;
            for (int tries = 0; tries < 2; tries++) {
                float dx = vel[0] * dt, dy = vel[1] * dt;
                float d = sqrtf(dx * dx + dy * dy);
                if (d < 0.0001f) break;
                float a[3] = { pos[0], pos[1], pos[2] + 0.1f };
                float b[3] = { pos[0] + dx + dx / d * 0.45f, pos[1] + dy + dy / d * 0.45f, pos[2] + 0.1f };
                float hp[3], hn[3];
                if (!RayHit(a, b, hp, hn)) {
                    EntityRemove(me);
                    pos[0] += dx; pos[1] += dy;
                    EntityAdd(me);
                    break;
                }
                // a wall: slide along it like Source does, not a dead stop
                float n2[3] = { hn[0], hn[1], 0.0f };
                float nl = sqrtf(n2[0] * n2[0] + n2[1] * n2[1]);
                if (nl < 0.05f) { vel[0] = vel[1] = 0.0f; break; }
                n2[0] /= nl; n2[1] /= nl;
                ClipVel(vel, n2, 2);
            }
        }
    }
    wasStanding = standing;
}

// ---- enemy markers -----------------------------------------------------------
// Every hostile ped (same rule as the aim lock) projected to the screen with
// the game's own CSprite::CalcScreenCoors - through walls - plus the ones off
// screen as a direction. Drawn by the menu's overlay.
std::vector<EnemyMark> EnemyMarks() {
    std::vector<EnemyMark> out;
    const Settings& A = Active();
    void* me = PlayerPed();
    if (!A.enemyEsp || !me || *GameMenuActive) return out;
    char* pool = (char*)*PedPool; if (!pool) return out;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    const float* cp = CameraPos;
    const float* mp = &at<float>(me, ENT_POS);
    for (int i = 0; i < size; i++) {
        if (flags[i] & 0x80) continue;
        void* p = objs + i * PED_POOL_STRIDE;
        if (!IsHostile(p, me)) continue;
        const float* pp = &at<float>(p, ENT_POS);
        float dx = pp[0] - mp[0], dy = pp[1] - mp[1], dz = pp[2] - mp[2];
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist > 300.0f) continue;
        EnemyMark m{};
        m.dist = dist;
        m.locked = p == g_aimTarget;
        float chest[3] = { pp[0], pp[1], pp[2] + 0.45f };
        float eye[3] = { cp[0], cp[1], cp[2] };
        m.visible = LineOfSightClear(eye, chest, true, false, false, false, false, true, true);
        float head[3] = { pp[0], pp[1], pp[2] + 0.95f }, feet[3] = { pp[0], pp[1], pp[2] - 1.0f };
        float sh[3], sf[3], w, h;
        bool a = CalcScreenCoors(head, sh, &w, &h, false);
        bool b = CalcScreenCoors(feet, sf, &w, &h, false);
        if (a && b) {
            m.onScreen = true;
            m.x = sh[0]; m.top = sh[1]; m.bottom = sf[1];
        } else {
            // off screen: which way, seen from the camera (x right, y forward)
            const float* cr = CameraRight; const float* cf = CameraFront;
            float vx = pp[0] - cp[0], vy = pp[1] - cp[1], vz = pp[2] - cp[2];
            m.dirX = vx * cr[0] + vy * cr[1] + vz * cr[2];
            m.dirY = vx * cf[0] + vy * cf[1] + vz * cf[2];
        }
        out.push_back(m);
    }
    return out;
}

// ---- never fall off bike -------------------------------------------------
// The knock-off hook reports every time the game throws the player off a bike;
// the game finishes the fall normally and we put the player straight back on.
static void* g_kbBike = nullptr;
static DWORD g_kbTime = 0;
static float g_kbMove[3];
static bool  g_forcingOff = false;     // our own ForceOffBike: don't re-seat
static DWORD g_exitPressTime = 0;      // last time the exit button was pressed on a bike
bool BikeExitRequested() { return g_exitPressTime && GetTickCount() - g_exitPressTime < 2000; }

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

// Roll stabiliser: never lets the bike lean past maxLean (radians) to either
// side. Only the roll speed is steered - smoothly, never a snap - so riding,
// wheelies and jumps are untouched, and a bike that would tip over (on the
// ground or in the air) is held up and lands on its wheels.
static constexpr float BIKE_MAX_LEAN = 0.9f;     // ~52 degrees
static void StabiliseBike(void* bike) {
    const float* r = &at<float>(bike, ENT_RIGHT);
    const float* f = &at<float>(bike, ENT_FWD);
    const float* u = &at<float>(bike, ENT_UP);
    float lean = atan2f(r[2], u[2]);              // 0 upright, +-pi upside down
    float excess = fabsf(lean) - BIKE_MAX_LEAN;
    if (excess <= 0.0f) return;
    float* w = &at<float>(bike, PHYS_TURN);       // world-space turn speed
    float roll = w[0] * f[0] + w[1] * f[1] + w[2] * f[2];
    // A positive roll about the forward axis lowers right.z, i.e. the lean.
    float sign = lean > 0 ? 1.0f : -1.0f;
    float want = sign * fminf(excess * 0.25f + 0.01f, 0.1f);
    if (roll * sign < want * sign) {              // not already coming back up fast enough
        float d = want - roll;
        for (int i = 0; i < 3; i++) w[i] += f[i] * d;
    }
}

static bool BikeTipped(void* bike) { return at<float>(bike, ENT_UP + 8) < 0.6f; }

static void StayOnBikeTick(const Settings& A) {
    void* ped = PlayerPed();
    if (A.stayOnBike && ped) {
        // Keep the ridden bike from leaning far enough to fall over.
        void* veh = PlayerVehicle();
        if (veh && at<int>(veh, VEH_TYPE) == 5 && at<void*>(veh, 0x1A8) == ped &&
            at<unsigned char>(ped, PED_IN_VEHICLE))
            StabiliseBike(veh);
    }
    void* bike = g_kbBike;
    if (!bike) return;
    DWORD now = GetTickCount();
    if (!A.stayOnBike || !ped || now - g_kbTime > 3000) { g_kbBike = nullptr; return; }
    if (now - g_exitPressTime < 2000) { g_kbBike = nullptr; return; } // player asked to get off
    void* cur = PlayerVehicle();
    if (cur && cur != bike) { g_kbBike = nullptr; return; }          // took another vehicle
    if (!BikeUsable(bike) || at<float>(ped, PED_HEALTH) <= 1.0f) { g_kbBike = nullptr; return; }
    // Thrown off anyway (crash): keep the bike from falling over meanwhile, and
    // if it already lies down, let it roll back up smoothly before re-seating.
    StabiliseBike(bike);
    if (cur == bike && at<unsigned char>(ped, PED_IN_VEHICLE)) return; // fall not finished yet
    bool lastChance = now - g_kbTime > 2500;
    if (BikeTipped(bike) && !lastChance) return;                      // still coming back up
    g_kbBike = nullptr;
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

    // Normally the stabiliser has it upright by now. Only if it is stuck on
    // its side for 2.5 s (wedged against something) is it set upright, as a
    // last resort, so the player is never left lying next to it.
    bool tipped = BikeTipped(bike);
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
            if (!S.autoLock) ShowNative("Doors unlocked so you can get back in");
        }
        g_trainerLocked = nullptr;
    }
    lastVeh = cur;
}

// Auto door lock: once the player is sitting in a vehicle that has doors
// (cars, vans, helicopters, planes - not bikes, boats or doorless cars), its
// doors are locked, once per entry, so changing it by hand afterwards sticks.
// Getting out unlocks them again (UnlockOnExit). Cars that were already
// locked (missions) are left alone and stay locked.
static bool VehicleHasDoors(void* v) {
    if (at<int>(v, VEH_TYPE) != 0) return false;                       // bikes, boats
    void* h = at<void*>(v, VEH_HANDLING);
    return !(h && (at<int>(h, HANDLING_FLAGS) & HANDLING_NO_DOORS));
}

static void AutoLockTick(const Settings& A) {
    static void* handled = nullptr;
    void* ped = PlayerPed();
    void* v = PlayerVehicle();
    if (!v || !ped) { handled = nullptr; return; }
    if (!A.autoLock || v == handled) return;
    int st = at<int>(ped, PED_STATE);
    if (st != 0x32 && st != 0x33) return;                               // not seated yet
    handled = v;
    if (!VehicleHasDoors(v) || VehicleWrecked(v)) return;
    int lock = at<int>(v, VEH_DOORLOCK);
    if (lock == 0 || lock == 1) {                                       // none / unlocked
        at<int>(v, VEH_DOORLOCK) = 2;
        g_trainerLocked = v;
    }
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

// ---- free landing spot ------------------------------------------------------
// Before any teleport the landing spot is checked against the real world:
// walls, fences, lamp posts (line-of-sight tests across the footprint) and
// other vehicles and people (their boxes). If the spot is taken, the nearest
// free spot around it is used instead, on the same ground and reachable
// from the original spot without passing through a wall.

static Footprint EntityFootprint(void* e, bool vehicle) {
    Footprint fp = vehicle ? Footprint{ 2.6f, 1.1f, 3.0f, 0.6f, 1.2f } : Footprint{ 0.4f, 0.4f, 0.6f, 1.0f, 0.9f };
    short m = at<short>(e, ENT_MODEL);
    if (m < 0 || m >= 6500) return fp;
    char* mi = (char*)ModelInfoPtrs[m];
    if (!mi || IsBadReadPtr(mi, MI_COLMODEL + 4)) return fp;
    char* col = *(char**)(mi + MI_COLMODEL);
    if (!col || IsBadReadPtr(col, COL_BOX_MAX + 12)) return fp;
    const float* mn = (const float*)(col + COL_BOX_MIN);
    const float* mx = (const float*)(col + COL_BOX_MAX);
    float hl = fmaxf(-mn[1], mx[1]), hw = fmaxf(-mn[0], mx[0]);
    float r = *(const float*)(col + COL_RADIUS);
    if (hl > 0.2f && hl < 25.0f && hw > 0.2f && hw < 12.0f) { fp.halfLen = hl; fp.halfWid = hw; }
    if (vehicle && -mn[2] > 0.1f && -mn[2] < 6.0f && mx[2] > 0.1f && mx[2] < 15.0f) { fp.bottom = -mn[2]; fp.top = mx[2]; }
    if (r > 0.3f && r < 35.0f) fp.radius = r;
    return fp;
}

// 2D boxes overlap? (centre, unit forward, half length/width) - separating axes.
static bool BoxesOverlap(const float* ca, float fax, float fay, float la, float wa,
                         const float* cb, float fbx, float fby, float lb, float wb) {
    float axes[4][2] = { { fax, fay }, { fay, -fax }, { fbx, fby }, { fby, -fbx } };
    float dx = cb[0] - ca[0], dy = cb[1] - ca[1];
    for (auto& a : axes) {
        auto proj = [&](float fx, float fy, float l, float w) {
            return fabsf((fx * a[0] + fy * a[1]) * l) + fabsf((fy * a[0] - fx * a[1]) * w);
        };
        if (fabsf(dx * a[0] + dy * a[1]) > proj(fax, fay, la, wa) + proj(fbx, fby, lb, wb)) return false;
    }
    return true;
}

// Water over the ground at x,y (ground height g)? Also water just around
// it, so a car isn't left on the very edge of a jetty or beach.
static bool InWater(float x, float y, float g) {
    for (int i = 0; i < 5; i++) {
        static const float o[5][2] = { { 0, 0 }, { 1.5f, 0 }, { -1.5f, 0 }, { 0, 1.5f }, { 0, -1.5f } };
        float px = x + o[i][0], py = y + o[i][1], wl = 0;
        if (!WaterLevelAt(px, py, g, &wl)) continue;
        bool f = false;
        float gz = i ? FindGroundZ3D(px, py, g + 2.0f, &f) : g;
        if (i && !f) return true;                       // nothing under it: open water
        if (wl > gz + 0.15f) return true;
    }
    return false;
}

// Vehicles only go where there is open sky: nothing overhead (roofs,
// garages, safehouse interiors, under bridges/overpasses)...
static bool Covered(float x, float y, float g, float head) {
    float a[3] = { x, y, g + head }, b[3] = { x, y, g + 60.0f };
    return !LineOfSightClear(a, b, true, false, false, false, false, false, false);
}
// ...and not up on a building: a level well above the dry ground around it
// is a rooftop (water around - a bridge or jetty - doesn't count).
static float GroundAround(float x, float y, float g, float r, int* dry, int* lower) {
    float low = g; *dry = *lower = 0;
    for (int i = 0; i < 8; i++) {
        float ang = 0.785398f * i;
        float px = x + cosf(ang) * r, py = y + sinf(ang) * r;
        bool f = false;
        float gz = FindGroundZ3D(px, py, g + 1.0f, &f);
        if (!f) continue;
        float wl;
        if (WaterLevelAt(px, py, gz, &wl) && wl > gz + 0.15f) continue;    // water
        (*dry)++;
        if (gz < g - 4.0f) (*lower)++;
        if (gz < low) low = gz;
    }
    return low;
}
static bool Rooftop(float x, float y, float g) {
    int dry, lower;
    GroundAround(x, y, g, 10.0f, &dry, &lower);
    if (lower >= 2) return true;
    GroundAround(x, y, g, 18.0f, &dry, &lower);
    return lower >= 3;
}

// Walls / solid world at a spot, measured from the ground under each point
// of the footprint (so slopes and kerbs don't count as walls):
//  - every footprint point must have ground close to the centre's ground
//    (a much higher "ground" there is a wall, ledge or building; none, or a
//    big drop, is an edge or water)
//  - lines between all those points, at two heights over the local ground,
//    must not hit buildings, objects (posts, fences) or dummies
//  - the body's height above the ground must be clear (no roof/overhang
//    through the car)
//  - not shut inside a building: something overhead plus walls on (nearly)
//    every side means an interior
static bool WorldClearAt(const float* c, float g0, float fx, float fy, const Footprint& fp, float L, float W) {
    float rx = fy, ry = -fx;
    const float sx[9] = { 0, 1, 1, -1, -1, 1, -1, 0, 0 };   // along: +front / -back
    const float sy[9] = { 0, 1, -1, -1, 1, 0, 0, 1, -1 };   // across: +right / -left
    float p[9][3];
    for (int i = 0; i < 9; i++) {
        p[i][0] = c[0] + fx * L * sx[i] + rx * W * sy[i];
        p[i][1] = c[1] + fy * L * sx[i] + ry * W * sy[i];
        if (fp.avoidWater && InWater(p[i][0], p[i][1], i ? g0 : g0)) return false;   // sea, river, pool
        if (i == 0) { p[i][2] = g0; continue; }
        bool f = false;
        float gz = FindGroundZ3D(p[i][0], p[i][1], g0 + 2.5f, &f);
        if (!f) return false;
        // Ground may only rise or fall like a road does (about 10%, plus a
        // kerb): a bigger step is a wall, barrier, ledge or drop.
        float d = sqrtf((p[i][0] - c[0]) * (p[i][0] - c[0]) + (p[i][1] - c[1]) * (p[i][1] - c[1]));
        if (fabsf(gz - g0) > 0.25f + 0.1f * d) return false;
        p[i][2] = gz;
    }
    {   // the same between neighbouring points round the outline
        static const int ring[8] = { 1, 5, 2, 8, 3, 6, 4, 7 };
        for (int i = 0; i < 8; i++) {
            const float* a = p[ring[i]]; const float* b = p[ring[(i + 1) % 8]];
            float d = sqrtf((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]));
            if (fabsf(a[2] - b[2]) > 0.25f + 0.1f * d) return false;
        }
    }
    auto clear = [](const float* a, const float* b) {
        return LineOfSightClear(a, b, true, false, false, true, true, false, false);
    };
    // Test lines follow the road's slope from the centre, not the local
    // "ground" (which is the top of a low wall when a point is on one).
    for (float h : { 0.25f, 0.7f, fminf(fp.bottom + fp.top, 2.2f) * 0.8f + 0.2f }) {
        float k[9][3];
        for (int i = 0; i < 9; i++) {
            float d = sqrtf((p[i][0] - c[0]) * (p[i][0] - c[0]) + (p[i][1] - c[1]) * (p[i][1] - c[1]));
            float lim = 0.1f * d, dz = p[i][2] - g0;
            k[i][0] = p[i][0]; k[i][1] = p[i][1];
            k[i][2] = g0 + h + (dz > lim ? lim : dz < -lim ? -lim : dz);
        }
        static const int ring[8] = { 1, 5, 2, 8, 3, 6, 4, 7 };   // round the outline
        for (int i = 0; i < 8; i++) {
            if (!clear(k[ring[i]], k[ring[(i + 1) % 8]])) return false;
            if (!clear(k[0], k[ring[i]])) return false;
        }
        if (!clear(k[1], k[3]) || !clear(k[2], k[4])) return false;   // diagonals
    }
    // headroom over the centre and the four corners
    float head = fp.bottom + fp.top + 0.3f;
    for (int i : { 0, 1, 2, 3, 4 }) {
        float a[3] = { p[i][0], p[i][1], p[i][2] + 0.3f }, b[3] = { p[i][0], p[i][1], p[i][2] + head };
        if (!clear(a, b)) return false;
    }
    // vehicles: open sky over the whole body, and not on a rooftop
    if (fp.openSky) {
        for (int i = 0; i < 9; i++) if (Covered(p[i][0], p[i][1], p[i][2], head)) return false;
        for (int i = 0; i < 9; i++) {      // second opinion: the highest surface there must be this one
            bool f = false;
            float top = FindGroundZ3D(p[i][0], p[i][1], 1000.0f, &f);
            if (f && top > p[i][2] + 0.6f) return false;
        }
        if (Rooftop(c[0], c[1], g0)) return false;
    }
    // inside a building?
    float up0[3] = { c[0], c[1], g0 + head }, up1[3] = { c[0], c[1], g0 + 40.0f };
    if (!LineOfSightClear(up0, up1, true, false, false, false, false, false, false)) {
        int walls = 0;
        float o[3] = { c[0], c[1], g0 + 1.2f };
        for (int i = 0; i < 8; i++) {
            float ang = 0.785398f * i;
            float e[3] = { o[0] + cosf(ang) * 15.0f, o[1] + sinf(ang) * 15.0f, o[2] };
            if (!LineOfSightClear(o, e, true, false, false, false, false, false, false)) walls++;
        }
        if (walls >= 7) return false;
    }
    return true;
}

static bool SpotFree(void* ent, void* ignore, const float* c, float g0, float fx, float fy, const Footprint& fp,
                     const float* from) {
    const float pad = fp.openSky ? 0.6f : 0.4f;
    float L = fp.halfLen + pad, W = fp.halfWid + pad;
    // vehicles and people in the way
    short n = 0; void* found[32] = {};
    FindObjectsKindaColliding(c, fp.radius + 3.0f, false, &n, 32, found, false, true, true, false, false);
    for (int i = 0; i < n && i < 32; i++) {
        void* o = found[i];
        if (!o || o == ent || o == ignore) continue;
        bool isPed = (at<unsigned char>(o, ENT_STATUS) & 7) == 3;           // ENTITY_TYPE_PED
        if (isPed && at<unsigned char>(o, PED_IN_VEHICLE)) continue;        // sitting in a vehicle: that vehicle counts
        if (isPed && ignore && at<void*>(o, PED_MY_VEHICLE) == ignore) continue;
        const float* op = &at<float>(o, ENT_POS);
        if (fabsf(op[2] - c[2]) > 3.0f) continue;                           // on a different level
        const float* of = &at<float>(o, ENT_FWD);
        float ol = sqrtf(of[0] * of[0] + of[1] * of[1]);
        float ofx = ol > 0.01f ? of[0] / ol : 0.0f, ofy = ol > 0.01f ? of[1] / ol : 1.0f;
        Footprint ofp = EntityFootprint(o, !isPed);
        if (BoxesOverlap(c, fx, fy, L, W, op, ofx, ofy, ofp.halfLen, ofp.halfWid)) return false;
    }
    if (!WorldClearAt(c, g0, fx, fy, fp, L, W)) return false;
    // reachable from the spot asked for, not through a wall
    if (from) {
        float a[3] = { from[0], from[1], from[2] + 0.6f }, b[3] = { c[0], c[1], g0 + 1.0f };
        if (!LineOfSightClear(a, b, true, false, false, false, false, false, false)) return false;
    }
    return true;
}

// On foot only a person's own space matters: ground to stand on, not in
// deep water, head room, and no wall through the body. Roofs overhead,
// bridges, alleys and edges are all fine.
static bool PedSpotOk(void* ped, const float* c, float g0) {
    float wl;
    if (WaterLevelAt(c[0], c[1], g0, &wl) && wl > g0 + 0.9f) return false;   // would be swimming
    float a[3] = { c[0], c[1], g0 + 0.3f }, b[3] = { c[0], c[1], g0 + 1.85f };
    if (!LineOfSightClear(a, b, true, false, false, true, false, false, false)) return false;   // no room to stand
    for (float h : { 0.5f, 1.4f }) {
        float o[3] = { c[0], c[1], g0 + h };
        for (int i = 0; i < 8; i++) {
            float ang = 0.785398f * i;
            float e[3] = { o[0] + cosf(ang) * 0.35f, o[1] + sinf(ang) * 0.35f, o[2] };
            if (!LineOfSightClear(o, e, true, false, false, true, false, false, false)) return false;
        }
    }
    short n = 0; void* found[16] = {};                                       // not inside a vehicle
    FindObjectsKindaColliding(c, 4.0f, false, &n, 16, found, false, true, false, false, false);
    for (int i = 0; i < n && i < 16; i++) {
        void* v = found[i];
        if (!v || v == ped) continue;
        const float* vp = &at<float>(v, ENT_POS);
        if (fabsf(vp[2] - c[2]) > 2.5f) continue;
        const float* vf = &at<float>(v, ENT_FWD);
        float vl = sqrtf(vf[0] * vf[0] + vf[1] * vf[1]);
        Footprint vfp = EntityFootprint(v, true);
        if (BoxesOverlap(c, 0, 1, 0.35f, 0.35f, vp, vl > 0.01f ? vf[0] / vl : 0, vl > 0.01f ? vf[1] / vl : 1,
                         vfp.halfLen, vfp.halfWid)) return false;
    }
    return true;
}

static int g_lastSpotMove = 0;     // metres the last teleport was moved (-1: no free spot, -2: only water)

// Finds a free spot at or around c (changed in place, height set to stand
// on the ground there). false: none found within 35 m, c is left as it was.
static bool FindFreeSpot(void* ent, void* ignore, float* c, float fx, float fy, bool vehicle) {
    if (!vehicle) {
        g_lastSpotMove = 0;
        bool ok = false;
        float g0 = FindGroundZ3D(c[0], c[1], c[2] + 1.0f, &ok);
        if (!ok) return false;
        if (PedSpotOk(ent, c, g0)) { c[2] = g0 + 1.0f; return true; }
        for (float r : { 0.6f, 1.2f, 2.0f, 3.0f, 4.5f, 6.0f, 8.0f }) {
            for (int i = 0; i < 12; i++) {
                float ang = 0.523599f * i;
                float t[3] = { c[0] + cosf(ang) * r, c[1] + sinf(ang) * r, 0 };
                bool g = false;
                float gz = FindGroundZ3D(t[0], t[1], g0 + 1.5f, &g);
                if (!g || fabsf(gz - g0) > 1.5f) continue;
                float from[3] = { c[0], c[1], g0 + 1.0f }, to[3] = { t[0], t[1], gz + 1.0f };
                if (!LineOfSightClear(from, to, true, false, false, false, false, false, false)) continue;
                if (PedSpotOk(ent, t, gz)) { c[0] = t[0]; c[1] = t[1]; c[2] = gz + 1.0f; g_lastSpotMove = (int)(r + 0.5f); return true; }
            }
        }
        c[2] = g0 + 1.0f;                                                    // nothing better: the spot itself
        return true;
    }
    Footprint fp = EntityFootprint(ent, vehicle);
    fp.avoidWater = !(vehicle && at<int>(ent, VEH_TYPE) == 1);              // boats are fine on water
    int vt = vehicle ? at<int>(ent, VEH_TYPE) : -1;
    fp.openSky = vehicle && vt != 3 && vt != 4;                              // helicopters / planes may use roofs
    g_lastSpotMove = 0;
    bool ok = false;
    float g0 = FindGroundZ3D(c[0], c[1], c[2] + 1.0f, &ok);
    float wl = 0;
    bool wet = fp.avoidWater && WaterLevelAt(c[0], c[1], c[2], &wl) && (!ok || wl > g0 + 0.15f);
    if (!ok && !wet) return false;                                           // nothing to stand on: leave it
    float above = ok ? fmaxf(c[2] - g0, fp.bottom + 0.15f) : fp.bottom + 0.15f;   // never sunk into the ground
    if (above > fp.bottom + 3.0f) above = fp.bottom + 0.15f;                 // asked for mid-air: stand on it
    // In water, look for land around the water's surface instead.
    float ref = wet ? wl : g0, tol = wet ? 10.0f : 2.5f;
    float base[3] = { c[0], c[1], ref + above };
    if (!wet && SpotFree(ent, ignore, base, g0, fx, fy, fp, nullptr)) { memcpy(c, base, 12); return true; }
    // A vehicle asked onto a rooftop or under a roof (a safehouse, a garage,
    // inside a building): look for open street level around it instead,
    // without the "reachable through no wall" rule (it starts indoors).
    const float* from = base;
    if (fp.openSky && !wet) {
        float head = fp.bottom + fp.top + 0.3f;
        if (Rooftop(c[0], c[1], g0)) {
            int dry, lower;
            ref = GroundAround(c[0], c[1], g0, 12.0f, &dry, &lower);
            tol = 3.0f;
            from = nullptr;
        } else if (Covered(c[0], c[1], g0, head)) {
            from = nullptr;
        }
    }

    // both ways the vehicle can face along its heading, then turned 90 degrees
    const float rings[] = { 2.0f, 3.5f, 5.0f, 7.0f, 9.0f, 12.0f, 16.0f, 21.0f, 27.0f, 35.0f, 45.0f, 60.0f, 80.0f };
    for (float r : rings) {
        if (!wet && r > 60.0f) break;                                        // dry: 60 m is far enough
        int steps = r < 6.0f ? 12 : r < 40.0f ? 24 : 36;
        for (int i = 0; i < steps; i++) {
            float ang = 6.2831853f * i / steps;
            float t[3] = { base[0] + cosf(ang) * r, base[1] + sinf(ang) * r, 0 };
            bool g = false;
            float gz = FindGroundZ3D(t[0], t[1], ref + (wet ? tol : 2.0f), &g);
            if (!g || fabsf(gz - ref) > tol) continue;                       // no ground / another level
            t[2] = gz + above;
            if (SpotFree(ent, ignore, t, gz, fx, fy, fp, from)) {
                memcpy(c, t, 12);
                g_lastSpotMove = (int)(r + 0.5f);
                return true;
            }
        }
    }
    if (wet) { g_lastSpotMove = -2; return false; }                          // only water: don't go
    g_lastSpotMove = -3;                                                    // nowhere clear: don't go
    return false;
}

// Teleport the way the game's own scripts do it (SET_CHAR/CAR_COORDINATES):
// load the destination's collision, then the scene, look the ground up only
// after that, find a free spot (no walls, vehicles or people in the way),
// clear it, and move the entity with its own Teleport (vtable slot 11). For a
// vehicle that takes it out of the world and puts it back in the new area,
// stops all movement and spin and resets the suspension. The game's version
// also turns a car to face north; the heading it had (or `heading`) is put
// back, level and upright. `ignore`: an entity allowed to be close (a target
// being followed).
void TeleportTo(float x, float y, float z, bool findGround, bool clearSpace, void* ignore, const float* heading) {
    void* ped = PlayerPed(); if (!ped) return;
    void* ent = PlayerVehicle(); if (!ent) ent = ped;
    float target[3] = { x, y, z };
    float xy[2] = { x, y };
    ColStoreLoadAt(xy);                    // collision for this spot
    LoadSceneCollision(target);            // request + load it now
    LoadScene(target);                     // buildings / models around it
    if (findGround) {
        bool found = false;
        float gz = FindGroundZ3D(x, y, 1000.0f, &found);
        target[2] = (found ? gz : FindGroundZ(x, y)) + 1.0f;
    }
    float fx = 0, fy = 1;
    bool veh = ent != ped;
    const float* f0 = heading ? heading : &at<float>(ent, ENT_FWD);
    float len = sqrtf(f0[0] * f0[0] + f0[1] * f0[1]);
    if (len > 0.01f) { fx = f0[0] / len; fy = f0[1] / len; }

    FindFreeSpot(ent, ignore, target, fx, fy, veh);
    if (g_lastSpotMove == -2) { ShowNative("Teleport cancelled: only water there, no dry land within 80 m"); return; }
    if (g_lastSpotMove == -3) { ShowNative("Teleport cancelled: no clear open spot for this vehicle near there"); return; }
    if (g_lastSpotMove > 0) ShowNative("Teleport: moved %d m to a clear spot", g_lastSpotMove);
    else if (g_lastSpotMove < 0) ShowNative("Teleport: no clear spot nearby");
    if (clearSpace) ClearSpaceForMissionEntity(target, ent);
    using Teleport_t = void(__thiscall*)(void*, float, float, float);
    ((Teleport_t)(*(void***)ent)[11])(ent, target[0], target[1], target[2]);
    if (veh) {
        float* r = &at<float>(ent, ENT_RIGHT); r[0] = fy; r[1] = -fx; r[2] = 0;
        float* f = &at<float>(ent, ENT_FWD);   f[0] = fx; f[1] = fy;  f[2] = 0;
        float* u = &at<float>(ent, ENT_UP);    u[0] = 0;  u[1] = 0;   u[2] = 1;
        memset(&at<float>(ent, PHYS_MOVE), 0, 12);
        memset(&at<float>(ent, PHYS_TURN), 0, 12);
    }
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
        case 3: if (*ObjectPool) ent = PoolGetObject(*ObjectPool, handle); break;    // object
        case 4: case 5: memcpy(b.pos, t + BLIP_POS, 12); break;                      // coord / contact point
        default: continue;                                                           // pickups etc.: skip
        }
        if (b.type >= 1 && b.type <= 3) {
            if (!ent) continue;
            memcpy(b.pos, &at<float>(ent, ENT_POS), 12);
        }
        out.push_back(b);
    }
    return out;
}

// The entity a car / person / object blip follows, looked up again right
// now (the list in the menu is from an earlier frame).
static void* BlipEntity(int index, int type) {
    if (index < 0 || index >= RADAR_COUNT) return nullptr;
    char* t = RadarTrace + index * RADAR_SIZE;
    if (!t[BLIP_INUSE] || *(int*)(t + BLIP_TYPE) != type) return nullptr;
    int handle = *(int*)(t + BLIP_HANDLE);
    switch (type) {
    case 1: return *VehiclePool ? PoolGetVehicle(*VehiclePool, handle) : nullptr;
    case 2: return *PedPool ? PoolGetPed(*PedPool, handle) : nullptr;
    case 3: return *ObjectPool ? PoolGetObject(*ObjectPool, handle) : nullptr;
    }
    return nullptr;
}

// Moving targets: read where the target is at the moment of the jump (not
// where the menu last saw it), put the player right behind it facing the
// same way, and give the player's vehicle the target's speed so the two
// carry on together instead of the target driving off.
static void TeleportToMovingTarget(void* tgt, int type) {
    void* ped = PlayerPed(); if (!ped) return;
    void* myVeh = PlayerVehicle();
    if (type == 2 && at<unsigned char>(tgt, PED_IN_VEHICLE) && at<void*>(tgt, PED_MY_VEHICLE)) {
        tgt = at<void*>(tgt, PED_MY_VEHICLE);                    // person in a car: follow the car
        type = 1;
    }
    const float* tp = &at<float>(tgt, ENT_POS);
    const float* tf = &at<float>(tgt, ENT_FWD);
    const float* tr = &at<float>(tgt, ENT_RIGHT);
    float fl = sqrtf(tf[0] * tf[0] + tf[1] * tf[1]);
    float fx = fl > 0.01f ? tf[0] / fl : 0.0f, fy = fl > 0.01f ? tf[1] / fl : 1.0f;
    float mv[3] = {}, tv[3] = {};
    memcpy(mv, &at<float>(tgt, PHYS_MOVE), 12);
    memcpy(tv, &at<float>(tgt, PHYS_TURN), 12);
    float speed = sqrtf(mv[0] * mv[0] + mv[1] * mv[1]);

    // gap behind the target, centre to centre: both lengths and a little room
    Footprint tfp = EntityFootprint(tgt, type == 1);
    Footprint mfp = EntityFootprint(myVeh ? myVeh : ped, myVeh != nullptr);
    float back = tfp.halfLen + mfp.halfLen + (myVeh ? 1.0f : 0.6f);
    float x, y, z = tp[2];
    if (!myVeh && type == 1 && speed < 0.02f) {
        // parked car, on foot: stand at the driver's door
        x = tp[0] - tr[0] * (tfp.halfWid + 0.9f); y = tp[1] - tr[1] * (tfp.halfWid + 0.9f);
    } else {
        x = tp[0] - fx * back; y = tp[1] - fy * back;
    }
    z += myVeh ? 0.4f : 1.0f;
    float head[2] = { fx, fy };
    // no ClearSpaceForMissionEntity here: it could shove the target itself away
    TeleportTo(x, y, z, false, false, tgt, myVeh ? head : nullptr);

    if (myVeh && type == 1 && g_lastSpotMove > -2) {     // following a vehicle: same speed and turn
        memcpy(&at<float>(myVeh, PHYS_MOVE), mv, 12);
        float* t = &at<float>(myVeh, PHYS_TURN); t[0] = t[1] = 0; t[2] = tv[2];
    }
}

void TeleportToBlip(const Blip& b) {
    float x = b.pos[0], y = b.pos[1], z = b.pos[2];
    if (b.type >= 1 && b.type <= 3) {
        if (void* tgt = BlipEntity(b.index, b.type)) { TeleportToMovingTarget(tgt, b.type); return; }
        x += 3.0f; TeleportTo(x, y, z + 1.0f, false); return;   // gone: where it was last seen
    }
    bool noHeight = z < -50.0f || z == 0.0f;
    TeleportTo(x, y, noHeight ? z : z + 1.0f, noHeight);   // real height: stand on it
}

std::vector<Pickup> ActivePickups() {
    std::vector<Pickup> out;
    for (int i = 0; i < PICKUP_COUNT; i++) {
        char* e = PickupArray + i * PICKUP_SIZE;
        if (!e[PICKUP_TYPE] || e[PICKUP_REMOVED]) continue;      // free slot / collected
        Pickup p{ i, *(short*)(e + PICKUP_MODEL), {} };
        memcpy(p.pos, e, 12);
        if (p.pos[0] == 0.0f && p.pos[1] == 0.0f) continue;
        out.push_back(p);
    }
    return out;
}

// Straight onto the pickup on foot (so it is collected); a vehicle gets the
// nearest clear spot next to it.
void TeleportToPickup(const Pickup& p) {
    TeleportTo(p.pos[0], p.pos[1], p.pos[2] + 1.0f, false);
}

// A person already in, or on the way into, this vehicle.
static bool PedBoarding(void* p, void* veh) {
    if (!veh) return false;
    if (at<unsigned char>(p, PED_IN_VEHICLE)) return at<void*>(p, PED_MY_VEHICLE) == veh;
    int obj = at<int>(p, PED_OBJECTIVE);
    return (obj == 0x11 || obj == 0x12) && at<void*>(p, PED_CAR_IN_OBJECTIVE) == veh;
}

// The mission-marker hotkey. Never jumps to a marker the player is already
// at (that looped on the drop-off point). In an ambulance it does the
// paramedic round: patients (person markers) until the ambulance is full,
// then the drop-off (the destination marker), then patients again.
bool TeleportToMarker() {
    constexpr float HERE = 12.0f;                    // "already there"
    float me[3] = {};
    bool have = GetPlayerPos(me);
    void* veh = PlayerVehicle();
    auto dist = [&](const Blip& b) { return have ? sqrtf((b.pos[0] - me[0]) * (b.pos[0] - me[0]) + (b.pos[1] - me[1]) * (b.pos[1] - me[1])) : 1e9f; };

    std::vector<Blip> people, drops, others;
    bool boarding = false;
    for (auto& b : ActiveBlips()) {
        if (b.sprite != 0 || b.type == 5) continue;  // icons, race / event points
        if (b.type == 2) {
            void* p = BlipEntity(b.index, 2);
            if (p && PedBoarding(p, veh)) {          // ours already: not a target
                if (!at<unsigned char>(p, PED_IN_VEHICLE)) boarding = true;
                continue;
            }
            people.push_back(b);
        } else if (b.type == 4) drops.push_back(b);
        else others.push_back(b);
    }
    auto nearest = [&](const std::vector<Blip>& l) -> const Blip* {
        const Blip* best = nullptr;
        for (auto& b : l) if (!best || dist(b) < dist(*best)) best = &b;
        return best;
    };

    if (veh && at<short>(veh, ENT_MODEL) == MODEL_AMBULANCE) {
        int seats = at<unsigned char>(veh, VEH_MAX_PASSENGERS);
        int taken = at<unsigned char>(veh, VEH_NUM_PASSENGERS) + at<signed char>(veh, VEH_NUM_GETTING_IN);
        bool full = seats > 0 && taken >= seats;
        if (boarding) { ShowNative("Ambulance: waiting for the patient to get in"); return true; }
        const Blip* patient = nearest(people);
        const Blip* drop = nearest(drops);
        if (!full && patient) {
            if (dist(*patient) < HERE) { ShowNative("Ambulance: the patient is right here - wait for them"); return true; }
            TeleportToBlip(*patient);
            return true;
        }
        if (drop && (full || taken > 0 || !patient)) {
            if (dist(*drop) < HERE) { ShowNative("Ambulance: at the drop-off - stop in the marker"); return true; }
            TeleportToBlip(*drop);
            return true;
        }
        if (!patient && !drop) { ShowNative("No mission marker on the radar"); return false; }
    }

    // Any other mission: the nearest marker that isn't where the player is.
    const Blip* pick = nullptr;
    bool anyHere = false;
    for (auto* l : { &people, &others, &drops })
        for (auto& b : *l) {
            if (dist(b) < HERE) { anyHere = true; continue; }
            if (!pick || dist(b) < dist(*pick)) pick = &b;
        }
    if (!pick) {
        ShowNative(anyHere ? "You are already at the mission marker" : "No mission marker on the radar");
        return false;
    }
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
    tog(P, "wanted_cap", "Lock wanted level (max stars)", &S.wantedCapOn);
    press(P, "wanted_cap_up", "Max stars +1", [] { S.wantedCap = S.wantedCap >= 6 ? 6 : S.wantedCap + 1; ShowNative("Max wanted level: %d", S.wantedCap); });
    press(P, "wanted_cap_down", "Max stars -1", [] { S.wantedCap = S.wantedCap <= 0 ? 0 : S.wantedCap - 1; ShowNative("Max wanted level: %d", S.wantedCap); });
    tog(P, "infinite_ammo", "Infinite ammo", &S.infiniteAmmo);
    tog(P, "no_reload", "No reload", &S.noReload);
    tog(P, "never_tired", "Infinite sprint", &S.neverTired);
    tog(P, "fast_reload", "Fast reload", &S.fastReload);
    tog(P, "fly", "Fly mode", &S.flyMode);
    tog(P, "aim_lock", "Aim lock-on", &S.aimLock);
    tog("Weapons", "wall_bang", "Wall bang (shoot through walls)", &S.wallBang);
    tog("Weapons", "rapid_fire", "Rapid fire", &S.rapidFire);
    tog("Weapons", "no_scope_sniper", "No-scope sniper", &S.noScopeSniper);
    tog("Weapons", "sniper_auto", "Sniper full auto", &S.sniperAuto);
    tog(P, "super_jump", "Super jump", &S.superJump);
    tog(P, "super_run", "Super run", &S.superRun);
    tog(P, "auto_bhop", "Auto bunny hop", &S.autoBhop);
    tog(P, "no_landing_roll", "No landing roll", &S.noLandingRoll);
    tog("World", "genocide", "Genocide mode", &S.genocide);
    tog(P, "no_ragdoll", "No ragdoll (never knocked down)", &S.noRagdoll);
    tog("Weapons", "one_shot_vehicles", "One-shot vehicles", &S.oneShotVehicles);
    tog(P, "enemy_esp", "Show enemies through walls", &S.enemyEsp);
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
    tog(V, "touch_kill", "Kill peds on one touch", &S.touchKill);
    tog(V, "auto_flip", "Auto flip car when on its roof", &S.autoFlip);
    tog(V, "survive_blowup", "Survive vehicle explosions", &S.surviveBlowUp);
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
    tog(V, "auto_lock", "Auto lock doors (unlock on exit)", &S.autoLock);
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
    {"enabled", &S.enabled}, {"stay_on_bike", &S.stayOnBike}, {"heavy_car", &S.heavyCar}, {"touch_kill", &S.touchKill}, {"aim_lock", &S.aimLock}, {"wanted_cap", &S.wantedCapOn}, {"wall_bang", &S.wallBang}, {"rapid_fire", &S.rapidFire}, {"no_scope_sniper", &S.noScopeSniper}, {"sniper_auto", &S.sniperAuto}, {"super_jump", &S.superJump}, {"super_run", &S.superRun}, {"auto_bhop", &S.autoBhop}, {"bhop_surf", &S.bhopSurf}, {"no_landing_roll", &S.noLandingRoll}, {"genocide", &S.genocide}, {"no_ragdoll", &S.noRagdoll}, {"one_shot_vehicles", &S.oneShotVehicles}, {"enemy_esp", &S.enemyEsp}, {"auto_flip", &S.autoFlip}, {"auto_lock", &S.autoLock}, {"survive_blowup", &S.surviveBlowUp},
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
        if (!strcmp(key, "bhop_mode")) S.bhopMode = atoi(val) % 3;
        if (!strcmp(key, "bhop_gain")) S.bhopGain = fminf(fmaxf((float)atof(val), 1.0f), 1.5f);
        if (!strcmp(key, "bhop_max")) S.bhopMax = fminf(fmaxf((float)atof(val), 10.0f), 150.0f);
        if (!strcmp(key, "bhop_height")) S.bhopHeight = fminf(fmaxf((float)atof(val), 0.5f), 4.0f);
        if (!strcmp(key, "wanted_cap_level")) { int v = atoi(val); S.wantedCap = v < 0 ? 0 : v > 6 ? 6 : v; }
        if (!strcmp(key, "super_jump_power")) S.superJumpPower = fminf(fmaxf((float)atof(val), 1.5f), 8.0f);
        if (!strcmp(key, "super_run_speed")) S.superRunSpeed = fminf(fmaxf((float)atof(val), 1.2f), 5.0f);
        if (!strcmp(key, "heavy_mode")) S.heavyMode = atoi(val) % 3;
        if (!strcmp(key, "blowup_mode")) S.blowUpMode = atoi(val) == 1 ? 1 : 0;
        if (!strcmp(key, "max_health")) { int v = atoi(val); S.maxHealth = v >= 1 && v <= 255 ? v : 0; }
        if (!strcmp(key, "max_armour")) { int v = atoi(val); S.maxArmour = v >= 1 && v <= 255 ? v : 0; }
    }
    fclose(f);
}

void SaveSettings() {
    // Remember the max health/armour the player has right now, however it got
    // there (trainer slider, an earlier session, a save game).
    if (PlayerPed()) {
        if (*MaxHealth > 100) S.maxHealth = *MaxHealth;
        if (*MaxArmour > 100) S.maxArmour = *MaxArmour;
    }
    FILE* f = fopen(kIni, "w");
    if (!f) return;
    fprintf(f, "# Modern VC Trainer settings (written by the trainer)\n");
    for (auto& b : kBools) fprintf(f, "%s = %d\n", b.key, *b.val ? 1 : 0);
    for (auto& h : HotkeyActions())
        if (h.vk) fprintf(f, "key_%s = %d\nkeymods_%s = %d\n", h.id.c_str(), h.vk, h.id.c_str(), h.mods);
    fprintf(f, "mouse_speed = %.2f\n", g_mouseSensitivity);
    fprintf(f, "fly_speed = %.1f\n", S.flySpeed);
    fprintf(f, "bhop_mode = %d\n", S.bhopMode);
    fprintf(f, "bhop_gain = %.2f\n", S.bhopGain);
    fprintf(f, "bhop_max = %.0f\n", S.bhopMax);
    fprintf(f, "bhop_height = %.1f\n", S.bhopHeight);
    fprintf(f, "wanted_cap_level = %d\n", S.wantedCap);
    fprintf(f, "super_jump_power = %.1f\n", S.superJumpPower);
    fprintf(f, "super_run_speed = %.1f\n", S.superRunSpeed);
    fprintf(f, "heavy_mode = %d\n", S.heavyMode);
    fprintf(f, "blowup_mode = %d\n", S.blowUpMode);
    if (S.maxHealth) fprintf(f, "max_health = %d\n", S.maxHealth);
    if (S.maxArmour) fprintf(f, "max_armour = %d\n", S.maxArmour);
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
    // The game's own suicide only does 1000 damage, which god mode, proofs or
    // a vehicle can stop: use the trainer's sure kill instead.
    if (!strcmp(code, "ICANTTAKEITANYMORE")) { KillPlayer(); return; }
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
// Weapons while flying on foot: the game won't fire or change weapon for a
// player hanging in the air, so the trainer does both. Mouse wheel picks the
// next / previous weapon you have (CPed::SetCurrentWeapon - model in hand
// and all); the fire button fires the game's own shot (CWeapon::Fire: aim
// along the crosshair, sound, ammo, reload) at the weapon's normal rate.
// A rocket or grenade fired while hovering appears inside the player's own
// body, hits it and goes off there (a detonator bomb sticks to it). Anything
// new of that kind showing up within reach right after a shot is moved out in
// front, keeping its speed (one with none gets a toss along the camera).
static void PushProjectilesOut(void* ped) {
    static void* seen[32]; static int nSeen = 0;
    static int lastTimer = 0, lastSlot = -1; static DWORD lastShot = 0;
    int slot = at<signed char>(ped, PED_CUR_WEAPON);
    if (slot >= 0 && slot < WEAPON_SLOTS) {
        int t = at<int>((char*)ped + PED_WEAPONS + slot * WEAPON_SIZE, WEAP_TIMER);
        if (slot == lastSlot && t != lastTimer) lastShot = GetTickCount();
        lastTimer = t; lastSlot = slot;
    }
    if (GetTickCount() - lastShot > 500) return;                            // only right after a shot / throw
    char* pool = (char*)*ObjectPool;
    if (!pool) return;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    const float* pp = &at<float>(ped, ENT_POS);
    float cf[3] = { CameraFront[0], CameraFront[1], CameraFront[2] };
    float cl = sqrtf(cf[0] * cf[0] + cf[1] * cf[1] + cf[2] * cf[2]);
    if (cl < 0.01f) return;
    for (float& v : cf) v /= cl;
    for (int i = 0; i < size; i++) {
        if (flags[i] & 0x80) continue;
        void* o = objs + i * OBJECT_POOL_STRIDE;
        short m = at<short>(o, ENT_MODEL);
        if (m != MODEL_MISSILE && m != 270 && m != 271 && m != 272 && m != 291) continue;   // rocket, grenades, molotov, bomb
        bool known = false;
        for (int k = 0; k < nSeen && k < 32; k++) if (seen[k] == o) known = true;
        if (known) continue;
        float* op = &at<float>(o, ENT_POS);
        float dx = op[0] - pp[0], dy = op[1] - pp[1], dz = op[2] - pp[2];
        if (dx * dx + dy * dy + dz * dz > 2.2f * 2.2f) continue;
        float* mv = &at<float>(o, PHYS_MOVE);
        float sp = sqrtf(mv[0] * mv[0] + mv[1] * mv[1] + mv[2] * mv[2]);
        float d[3] = { cf[0], cf[1], cf[2] };
        if (sp > 0.05f) for (int k = 0; k < 3; k++) d[k] = mv[k] / sp;
        else for (int k = 0; k < 3; k++) mv[k] = cf[k] * 0.4f;            // nothing to fly with: toss it
        EntityRemove(o);
        for (int k = 0; k < 3; k++) op[k] = pp[k] + d[k] * 2.6f;
        EntityAdd(o);
        seen[nSeen++ % 32] = o;
        if (nSeen >= 64) nSeen = 32;
    }
}

static void FlyWeapons(const Settings& A, void* ped) {
    PushProjectilesOut(ped);
    if (!GameFocused() || g_menuOpen || *GameMenuActive) return;
    int slot = at<signed char>(ped, PED_CUR_WEAPON);
    if (slot < 0 || slot >= WEAPON_SLOTS) slot = 0;
    auto usable = [&](int s) {
        char* w = (char*)ped + PED_WEAPONS + s * WEAPON_SIZE;
        int type = at<int>(w, WEAP_TYPE);
        if (s == 0) return true;                                           // fists
        return type != 0 && (at<int>(w, WEAP_TOTAL) > 0 || at<int>(w, WEAP_CLIP) > 0);
    };
    int dir = NewMouseState[3] ? 1 : NewMouseState[4] ? -1 : 0;           // wheel up / down
    if (dir) {
        for (int i = 1; i < WEAPON_SLOTS; i++) {
            int s = (slot + dir * i + WEAPON_SLOTS) % WEAPON_SLOTS;
            if (usable(s)) { SetCurrentWeapon(ped, s); slot = s; break; }
        }
    }
    if (!NewMouseState[0] || A.rapidFire) return;                          // rapid fire does its own shooting
    char* w = (char*)ped + PED_WEAPONS + slot * WEAPON_SIZE;
    int type = at<int>(w, WEAP_TYPE);
    char* wi = GetWeaponInfo(type);
    if (!wi || at<int>(wi, WI_FIRETYPE) == 0) return;                      // fists / melee: nothing to fire
    if (IsScopeWeapon(type) && (A.noScopeSniper || A.sniperAuto)) return;   // sniper / rocket: their own features
    if (at<int>(w, WEAP_STATE) == WEAPSTATE_RELOADING) return;
    if ((int)(at<int>(w, WEAP_TIMER) - (int)*TimeMs) > 0) return;          // between shots
    FireAnyView(w, ped);
    PushProjectilesOut(ped);
}

static void FlyTick() {
    const Settings& A = Active();
    static bool active = false;
    static void* ent = nullptr;
    static float pos[3];
    static LARGE_INTEGER freq, last;

    void* ped = PlayerPed();
    void* target = PlayerVehicle(); if (!target) target = ped;
    g_flyingOnFoot = A.flyMode && target && target == ped;

    // While flying nothing collides with the player (or the vehicle flown):
    // the entity's "uses collision" bit (CEntity +0x51 bit 0, what the
    // SET_..._COLLISION script command flips, 0x45E415) is off, so your own
    // rockets and grenades - even ones you overtake - pass through you. Put
    // back as it was when flying stops or you change body.
    {
        static void* noColl = nullptr;
        static bool hadColl = false;
        void* want = (A.flyMode && target) ? target : nullptr;
        if (noColl && noColl != want) {
            bool alive = noColl == ped || IsLiveVehicle(noColl);
            if (alive && hadColl) at<unsigned char>(noColl, ENT_FLAGS_51) |= 0x01;
            noColl = nullptr;
        }
        if (want && noColl != want) { noColl = want; hadColl = (at<unsigned char>(want, ENT_FLAGS_51) & 0x01) != 0; }
        if (noColl) at<unsigned char>(noColl, ENT_FLAGS_51) &= ~0x01;
    }

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
    if (ent == ped) {
        // On foot the game would see no ground under the player and switch to
        // falling, which blocks aiming and shooting. Keep the player counted
        // as standing, and step straight out of a fall / get-up if one began.
        at<unsigned char>(ped, PED_FLAGS_14C) |= 0x03;           // bIsStanding, bWasStanding
        int st = at<int>(ped, PED_STATE);
        if (st == PED_FALL || st == PED_GETUP) at<int>(ped, PED_STATE) = PED_IDLE;
        FlyWeapons(A, ped);
    }
}

// Car god mode, part two: the game's own "can be damaged" flag (VEH_FLAGS_1FB
// bit 0x40, what missions use for indestructible cars). Every BlowUpCar
// (car, bike, boat, heli/plane) returns at once when it is clear, and the
// vehicle damage code skips it too - so a tank shell, rocket or explosion
// can't blow the car up between two frames, which topping up health can't
// prevent. The bit is put back when god mode goes off or the player leaves.
static void CarGodFlagTick(const Settings& A, void* veh) {
    static void* godCar = nullptr;
    static bool hadBit = false;
    void* want = A.carGodMode ? veh : nullptr;
    if (godCar && godCar != want) {
        if (IsLiveVehicle(godCar) && hadBit) at<unsigned char>(godCar, VEH_FLAGS_1FB) |= 0x40;
        godCar = nullptr;
    }
    if (!want) return;
    if (godCar != want) { godCar = want; hadBit = (at<unsigned char>(want, VEH_FLAGS_1FB) & 0x40) != 0; }
    at<unsigned char>(want, VEH_FLAGS_1FB) &= ~0x40;
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
    KillTick(ped);
    if (S.maxHealth && *MaxHealth < S.maxHealth) *MaxHealth = (unsigned char)S.maxHealth;
    if (S.maxArmour && *MaxArmour < S.maxArmour) *MaxArmour = (unsigned char)S.maxArmour;
    // Save soon after the max changes (slider, game), not only when the menu
    // closes, so quitting the game with the menu open keeps it.
    {
        static unsigned char seenH = 0, seenA = 0;
        static DWORD changedAt = 0;
        if (*MaxHealth != seenH || *MaxArmour != seenA) {
            bool newer = (*MaxHealth > 100 && *MaxHealth != S.maxHealth) ||
                         (*MaxArmour > 100 && *MaxArmour != S.maxArmour);
            if (seenH || newer) changedAt = GetTickCount();
            seenH = *MaxHealth; seenA = *MaxArmour;
        }
        if (changedAt && GetTickCount() - changedAt > 1000) { changedAt = 0; SaveSettings(); }
    }

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
    if (HoldNoWanted()) {
        void* w = at<void*>(ped, PED_WANTED);
        if (w && (GetWanted() > 0 || at<int>(w, 0) > 0)) game::SetWantedLevel(w, 0);  // stars + chaos
        // A cop on foot still assigned to the player is taken off the chase
        // and goes back to walking about (like the game's ResetPolicePursuit).
        // Cops sitting in a vehicle are never touched: changing anything on
        // them makes them get out of their car.
        if (w) {
            for (int i = 0; i < 10; i++) {
                void*& cop = at<void*>(w, WANTED_COPS + i * 4);
                if (!cop || at<unsigned char>(cop, PED_IN_VEHICLE_FLAG)) continue;
                int st = at<int>(cop, PED_STATE);
                if (st == 0x32 || st == 0x33 || st == PED_DIE || st == PED_DEAD) continue;  // seated / dying
                at<unsigned char>(cop, COP_IN_PURSUIT) = 0;
                at<int>(cop, PED_OBJECTIVE) = 0;                    // OBJECTIVE_NONE
                at<int>(cop, PED_OBJECTIVE + 4) = 0;                // previous objective
                game::PedSetWanderPath(cop, rand() & 7);
                cop = nullptr;
            }
        }
    }

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
    AutoLockTick(A);
    HeavyCarTick(A);
    TouchKillTick(A);
    if (int cap = WantedCap(); cap >= 0) {                     // wanted level lock
        void* w = at<void*>(ped, PED_WANTED);
        if (w && at<int>(w, WANTED_LEVEL) > cap) SetWantedLevel(w, cap);
    }
    AimLockTick(A, ped);
    NoScopeSniperTick(A, ped);
    SniperAutoTick(A, ped);
    SuperMoveTick(A, ped);
    BhopTick(A, ped);
    ShotTick(A, ped);
    RapidFireTick(A, ped);
    GenocideTick(A, ped);
    AutoFlipTick(A);
    StayOnBikeTick(A);
    BikeExitFix(A);
    WreckTick(A);
    CopCarEntryTick();

    static Proofs lastPed, lastCar;
    static void* lastCarPtr = nullptr;
    ApplyProofs(ped, A.playerProofs, lastPed, false);

    CarGodFlagTick(A, PlayerVehicle());
    if (void* veh = PlayerVehicle()) {
        if (veh != lastCarPtr) { lastCar = Proofs(); lastCarPtr = veh; }
        ApplyProofs(veh, A.carProofs, lastCar, false);
        if (A.carGodMode && at<float>(veh, VEH_HEALTH) < 1000.0f) at<float>(veh, VEH_HEALTH) = 1000.0f;
        // Looks at the whole damage state, not only health, so dents, tyres
        // and doors are fixed too - also with car god mode holding health at 1000.
        // The full repair also puts every door back in its closed position,
        // so it waits while anyone is getting in or out (a ped/cop opening the
        // door to pull the player out, or the player's own exit) - otherwise
        // the door snaps shut mid-animation. Health is still topped up.
        if (A.autoRepair && !VehicleWrecked(veh) && VehicleDamaged(veh)) {
            bool doorBusy = at<unsigned char>(veh, VEH_NUM_GETTING_IN) ||
                            at<unsigned char>(veh, VEH_GETTING_IN_FLAGS) ||
                            at<unsigned char>(veh, VEH_GETTING_OUT_FLAGS);
            static DWORD lastFix = 0;
            DWORD now = GetTickCount();
            if (doorBusy || now - lastFix < 500) at<float>(veh, VEH_HEALTH) = 1000.0f;
            else { FixVehicle(veh); lastFix = now; }
        }
    } else {
        lastCarPtr = nullptr;
    }

    if (A.freezeTime) {
        // Stop the clock itself, not just snap the hour/minute back: otherwise
        // the seconds keep running and the minute keeps rolling over, and the
        // weather/lighting blend (driven by minutes + seconds) jumps every
        // game minute.
        *ClockHours = (unsigned char)A.frozenHour;
        *ClockMinutes = (unsigned char)A.frozenMinute;
        *ClockSeconds = 0;
        *ClockLastTick = *TimeMs;
        // The game still computes a second or so from the frame time each
        // frame; if the weather's blend value then dropped back, the weather
        // would advance as if an hour had passed. Pin it at the frozen time
        // with zero seconds, the lowest value it can compute, so it never drops.
        *WeatherInterp = A.frozenMinute / 60.0f;
    }

    // Rampages keep their own clock (CDarkel): hold the time left where it is.
    {
        static bool held = false; static int left = 0;
        if (A.freezeMissionTimer && *FrenzyStatus == 1 && *FrenzyTimeLimit >= 0) {
            int now = (int)(*TimeMs - *FrenzyStart);
            int r = *FrenzyTimeLimit - now;
            if (!held || r > left) { held = true; left = r; }        // started, or time was added
            else *FrenzyStart = *TimeMs - (unsigned)(*FrenzyTimeLimit - left);
        } else held = false;
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
