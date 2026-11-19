// Vehicles: the list and handling data, spawning, paint, repair and flip,
// police car entry while never wanted, surviving explosions, the jump-out and
// wreck exit, auto flip, door locks, heavy car, never fall off bike, extras
// (rainbow, tyres, performance), delete vehicle, the garage and car god mode.
#include "features_internal.h"

// ------------------------------------------------------------------ vehicles
static std::vector<VehicleEntry> g_vehicles;
int g_lastSpawned = 141; // Infernus until something is spawned

static void WarpPlayerInto(void* veh) {
    void* ped = PlayerPed();
    if (!ped || !veh) return;
    PedSetObjective(ped, 0x12, veh);   // OBJECTIVE_ENTER_CAR_AS_DRIVER, as opcode 0369 does
    WarpPedIntoCar(ped, veh);
}

std::string Trim(std::string s) {
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
    FILE* f = OpenGameFile("data\\default.ide", "r");
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
        g_vehicles.push_back({ id, cat, name + "  (" + col[1] + ")", col[4], col.size() > 7 ? col[7] : "" });
    }
    fclose(f);
    return g_vehicles;
}

const VehicleSpec* VehicleSpecOf(const std::string& handlingId) {
    static std::unordered_map<std::string, VehicleSpec> specs;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        if (FILE* f = OpenGameFile("data\\handling.cfg", "r")) {
            char line[512];
            while (fgets(line, sizeof line, f)) {
                // A id, B mass, C-E size, F-H centre of mass, I submerged, J traction, K loss, L bias,
                // M gears, N max velocity, O engine accel, P drive, Q engine, R brake ... Z value, ae flags
                if (!isalpha((unsigned char)line[0])) continue;      // comments and the boat / bike / flying lines
                char id[32], drive = 0, engine = 0; float v[12]; int gears = 0, value = 0; char flags[16] = "0";
                float f2[7];
                int n = sscanf(line, "%31s %f %f %f %f %f %f %f %f %f %f %f %d %f %f %c %c %f %f %f %f %f %f %f %f %d %f %f %f %f %15s",
                               id, &v[0], &f2[0], &f2[1], &f2[2], &f2[3], &f2[4], &f2[5], &v[1], &v[2], &v[3], &v[4], &gears, &v[5], &v[6],
                               &drive, &engine, &v[7], &v[8], &f2[6], &v[9], &v[10], &v[11], &f2[0], &f2[1], &value,
                               &f2[2], &f2[3], &f2[4], &f2[5], flags);
                if (n < 26) continue;
                VehicleSpec s;
                s.mass = v[0]; s.traction = v[2]; s.gears = gears; s.maxKmh = v[5]; s.accel = v[6];
                s.drive = drive; s.engine = engine; s.brake = v[7]; s.value = value;
                if (n >= 31) s.flags = (unsigned)strtoul(flags, nullptr, 16);
                specs[id] = s;
            }
            fclose(f);
        }
    }
    auto it = specs.find(handlingId);
    return it == specs.end() ? nullptr : &it->second;
}

// Seat swap from one car to a newly spawned one: the camera picks what it
// follows when the player goes between on foot and in a vehicle, so going
// straight from car to car it kept following the old one. The player leaves
// the old seat at once and is warped into the new car a couple of frames
// later (PendingWarpTick), after the camera has seen them on foot.
static void* g_pendingWarp = nullptr;
static int g_pendingFrames = 0;
void SpawnVehicle(int model) { SpawnVehicleObj(model, true); }
void* SpawnVehicleObj(int model, bool warp) {
    void* ped = PlayerPed(); if (!ped || model <= 0 || model >= 6500 || !ModelInfoPtrs[model]) return nullptr;
    int type = at<int>(ModelInfoPtrs[model], MI_VEHTYPE);

    unsigned char* si = StreamInfo + model * 20;
    bool keep = (si[1] & 1) != 0;
    RequestModel(model, 1);
    LoadAllRequested(false);
    if (si[0] != 1) {
        ShowNative("Could not load that vehicle model");
        Log("Spawn: vehicle model %d not loaded (streaming state %d) - nothing spawned", model, si[0]);
        return nullptr;
    }
    if (!keep) { SetModelDeletable(model); SetModelTxdDeletable(model); }

    void* veh = nullptr;
    if (type == 5)      { if (void* m = VehicleNew(0x4EC)) veh = BikeCtor(m, model, 1); }
    else if (type == 1) { if (void* m = VehicleNew(0x4C0)) veh = BoatCtor(m, model, 1); }
    else                { if (void* m = VehicleNew(0x5DC)) veh = AutomobileCtor(m, model, 1); }
    if (!veh) {
        int used, size;
        PoolUse(*VehiclePool, &used, &size);
        ShowNative("Vehicle not made - vehicle pool full (%d of %d used)", used, size);
        Log("Spawn refused: vehicle model %d - vehicle pool full, %d of %d slots used", model, used, size);
        return nullptr;
    }

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

    if (warp && S.warpIntoSpawned) {
        // Already in a vehicle: leave its seat directly (no teleport, nothing
        // around is cleared - that could remove the car just spawned) and
        // warp into the new one. Before, only the first spawn seated you.
        if (void* old = PlayerVehicle()) {
            if (at<void*>(old, VEH_DRIVER) == ped) {
                VehRemoveDriver(old);
                at<unsigned char>(old, ENT_STATUS) = (at<unsigned char>(old, ENT_STATUS) & 7) | 0x20;  // abandoned
            } else {
                VehRemovePassenger(old, ped);
            }
            if (void* anim = at<void*>(ped, PED_VEHICLE_ANIM)) at<float>(anim, ASSOC_DELTA) = -1000.0f;
            at<void*>(ped, PED_VEHICLE_ANIM) = nullptr;
            at<unsigned char>(ped, PED_IN_VEHICLE_FLAG) = 0;
            at<void*>(ped, PED_MY_VEHICLE) = nullptr;
            at<int>(ped, PED_STATE) = 1;                                   // PED_IDLE
            g_pendingWarp = veh; g_pendingFrames = 2;                      // in on a later frame
        } else {
            WarpPlayerInto(veh);
        }
    }
    return veh;
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
bool IsCopModel(int m) { return (m >= 1 && m <= 4) || (m >= 97 && m <= 104); }  // cop swat fbi army, vice1-8

void CopCarEntryTick() {
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
    FILE* f = OpenGameFile("data\\carcols.dat", "r");
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
void FixVehicle(void* veh) {
    at<float>(veh, VEH_HEALTH) = 1000.0f;
    int type = at<int>(veh, VEH_TYPE);
    if (type == 0)      { at<float>(veh, AUTO_FIRE_TIMER) = 0.0f; AutomobileFix(veh); }
    else if (type == 5) { at<float>(veh, BIKE_FIRE_TIMER) = 0.0f; BikeFix(veh); }
}

// Anything a repair would change: lost health, a burst tyre, a dented panel,
// a broken light or a damaged door (cars without doors keep theirs "missing").
bool VehicleDamaged(void* veh) {
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

bool VehicleWrecked(void* veh) {
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

// Getting out of a moving car, the game's own way. CPed::SetExitCar
// (0x516C70) lets the player out normally only below ~3.5 m/s (CanPedExitCar,
// speed^2 < 0.005 per step); otherwise it asks CanPedJumpOutCar (0x5B80C0),
// which allows the roll-out (jump out with the roll animation) only between
// speed^2 0.1 and 0.5 (~57-127 km/h). Outside that the car is braked first
// until one of the two fits. The jump-out check reads its two limits through
// pointers in its code (0x5B8103 low, 0x5B8113 high): pointed at the
// trainer's own limits, every upright car rolls you out at any speed above a
// walk - same animation, no braking. Pointed back when the trainer is off.
static void RestoreCameraJumpCut();
void PendingWarpTick() {
    if (!g_pendingWarp) return;
    if (--g_pendingFrames > 0) return;
    void* veh = g_pendingWarp;
    g_pendingWarp = nullptr;
    if (IsLiveVehicle(veh) && !PlayerVehicle()) {
        WarpPlayerInto(veh);
        // The camera was left in on-foot mode aimed at the player, now
        // sitting inside the car. Script opcode 02EB (restore_camera_jumpcut)
        // re-picks the camera from what the player is in: behind the car.
        RestoreCameraJumpCut();
    }
}

static const float kJumpOutMin = 0.005f, kJumpOutMax = 1.0e9f;
void FastExitTick(const Settings& A) {
    static int applied = -1;
    int want = A.enabled ? 1 : 0;
    if (want == applied) return;
    void** lo = (void**)0x5B8103, **hi = (void**)0x5B8113;
    void* origLo = (void*)0x69C6E8, *origHi = (void*)0x69C668;
    if (applied == -1 && (*lo != origLo || *hi != origHi) &&
        (*lo != (void*)&kJumpOutMin || *hi != (void*)&kJumpOutMax)) { applied = want; return; }   // unknown code: leave it
    DWORD old;
    VirtualProtect((void*)0x5B8100, 0x18, PAGE_EXECUTE_READWRITE, &old);
    *lo = want ? (void*)&kJumpOutMin : origLo;
    *hi = want ? (void*)&kJumpOutMax : origHi;
    VirtualProtect((void*)0x5B8100, 0x18, old, &old);
    applied = want;
}

// Sitting in a wreck: kept safe from its fire, and the exit button always
// works - if the game won't start getting out within 0.3 s, the player is
// set down beside it.
void WreckTick(const Settings& A) {
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

// Auto flip (cars and boats, not bikes/helis/planes): as soon as the player's
// car is upside down AND touching the ground, turn it back over - it may still
// be sliding, and keeps that speed. In the air it is never flipped: the ground
// below the car (traced down from the car, so bridges above don't count) must
// be within reach of the roof.
void AutoFlipTick(const Settings& A) {
    static DWORD invertedSince = 0, lastFlip = 0;
    void* veh = PlayerVehicle();
    // VEH_TYPE 0 = CAutomobile, which also covers helis/planes; the model info
    // type (0 car, 1 boat, 3 heli, 4 plane, 5 bike) narrows it to real cars.
    bool car = veh && at<int>(veh, VEH_TYPE) == 0 &&
               at<int>(ModelInfoPtrs[at<short>(veh, ENT_MODEL)], MI_VEHTYPE) == 0;
    bool boat = veh && at<int>(veh, VEH_TYPE) == 1;                            // CBoat
    if (!A.autoFlip || !(car || boat) || at<float>(veh, VEH_HEALTH) <= 0.0f) {
        invertedSince = 0; return;
    }
    if (at<float>(veh, ENT_UP + 8) >= -0.2f) { invertedSince = 0; return; }   // wheels / hull not up

    const float* p = &at<float>(veh, ENT_POS);
    bool found = false;
    float ground = FindGroundZ3D(p[0], p[1], p[2] + 0.5f, &found);
    // Car centre is about 0.5-1 m above the ground when it lies on its roof.
    bool onGround = found && p[2] - ground < 1.8f && at<float>(veh, PHYS_MOVE + 8) > -0.15f;
    // A capsized boat floats hull-up on the water (or lies on a beach).
    float wl = 0.0f;
    if (boat && !onGround && WaterLevelAt(p[0], p[1], p[2], &wl))
        onGround = p[2] - wl < 2.5f && p[2] - wl > -4.0f;
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
static int g_lockRestore = 1;          // the lock it gets back when the player leaves
void DoorLockBeforeInput();

void SetDoorLock(bool locked) {
    void* v = PlayerVehicle();
    if (!v) { ShowNative("You are not in a vehicle"); return; }
    // 4 = CARLOCK_LOCKED_PLAYER_INSIDE: nobody gets in, and - unlike plain
    // "locked" (2) - nobody can pull the player out either (the game's
    // carjack code, 0x4BD7C4 / 0x4BD80D, skips a car locked this way).
    at<int>(v, VEH_DOORLOCK) = locked ? 4 : 1;
    g_trainerLocked = locked ? v : nullptr;
    g_lockRestore = 1;
    ShowNative(locked ? "Doors locked" : "Doors unlocked");
}

// True if p is a vehicle that still exists in the game's vehicle pool.
bool IsLiveVehicle(void* p) {
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
// Cars and motorbikes only: the model info type (0 car, 1 boat, 3 heli,
// 4 plane, 5 bike) leaves out boats, helicopters and planes.
static bool IsCarOrBike(void* v) {
    int mi = at<short>(v, ENT_MODEL);
    if (mi < 0 || mi >= MODEL_COUNT || !ModelInfoPtrs[mi]) return false;
    int t = at<int>(ModelInfoPtrs[mi], MI_VEHTYPE);
    return t == 0 || t == 5;
}

void HeavyCarTick(const Settings& A) {
    static void* heavy = nullptr;        // car whose mass we changed
    static float origMass = 0, origTurnMass = 0;
    static std::vector<std::pair<void*, DWORD>> hitTimes;

    void* veh = PlayerVehicle();
    bool want = A.heavyCar && veh && IsCarOrBike(veh);

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
        if (!IsCarOrBike(o)) continue;                              // boats, helis, planes
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

void StayOnBikeTick(const Settings& A) {
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
void BikeExitFix(const Settings& A) {
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

// The "player inside" lock (4) also stops the player getting out. So the
// moment the exit button goes down - from the pad hook, before the game reads
// the press this frame - the car drops to plain "locked" (2), which still keeps
// everyone out but lets the player leave. If they stay in (let go without
// getting out) it goes back to 4 (DoorLockRelock, every tick).
static bool g_exitUnlocked = false;
void DoorLockBeforeInput() {
    void* v = PlayerVehicle();
    if (!v || v != g_trainerLocked || g_menuOpen) return;
    if (at<short>(Pad0, 0x1E) == 0 || at<short>(Pad0, 0xF0) != 0) return;   // exit not pressed / controls off
    if (at<int>(v, VEH_DOORLOCK) == 4) { at<int>(v, VEH_DOORLOCK) = 2; g_exitUnlocked = true; }
}
void DoorLockRelock() {
    static DWORD releasedAt = 0;
    void* v = PlayerVehicle();
    if (!g_exitUnlocked) { releasedAt = 0; return; }
    if (!v || v != g_trainerLocked) { g_exitUnlocked = false; return; }     // got out: UnlockOnExit takes over
    void* ped = PlayerPed();
    int st = ped ? at<int>(ped, PED_STATE) : 0;
    if (at<short>(Pad0, 0x1E) != 0 || (st != 0x32 && st != 0x33)) { releasedAt = 0; return; }  // still pressing / getting out
    if (!releasedAt) { releasedAt = GetTickCount(); return; }
    if (GetTickCount() - releasedAt < 500) return;                           // give the exit a moment to start
    if (at<int>(v, VEH_DOORLOCK) == 2) at<int>(v, VEH_DOORLOCK) = 4;          // stayed in: protected again
    g_exitUnlocked = false; releasedAt = 0;
}

void UnlockOnExit() {
    static void* lastVeh = nullptr;
    void* cur = PlayerVehicle();
    if (lastVeh && cur != lastVeh && lastVeh == g_trainerLocked) {
        if (IsLiveVehicle(lastVeh) && (at<int>(lastVeh, VEH_DOORLOCK) == 4 || at<int>(lastVeh, VEH_DOORLOCK) == 2)) {
            at<int>(lastVeh, VEH_DOORLOCK) = g_lockRestore;           // unlocked, or a mission car's own lock
            if (!S.autoLock && g_lockRestore == 1) ShowNative("Doors unlocked so you can get back in");
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

void AutoLockTick(const Settings& A) {
    static void* handled = nullptr;
    void* ped = PlayerPed();
    void* v = PlayerVehicle();
    if (!v || !ped) { handled = nullptr; return; }
    if (!A.autoLock || v == handled) return;
    int st = at<int>(ped, PED_STATE);
    if (st != 0x32 && st != 0x33) return;                               // not seated yet
    handled = v;
    if (!VehicleHasDoors(v) || VehicleWrecked(v)) return;
    // Locked "with the player inside" (4): no one gets in and no one can drag
    // the player out - carjackers, cops, vigilante targets the player was
    // teleported next to. A mission car already locked (2) gets the same
    // while the player sits in it, and its own lock back after.
    int lock = at<int>(v, VEH_DOORLOCK);
    if (lock == 0 || lock == 1 || lock == 2) {
        g_lockRestore = lock == 2 ? 2 : 1;
        at<int>(v, VEH_DOORLOCK) = 4;
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

// Car god mode, part two: the game's own "can be damaged" flag (VEH_FLAGS_1FB
// bit 0x40, what missions use for indestructible cars). Every BlowUpCar
// (car, bike, boat, heli/plane) returns at once when it is clear, and the
// vehicle damage code skips it too - so a tank shell, rocket or explosion
// can't blow the car up between two frames, which topping up health can't
// prevent. The bit is put back when god mode goes off or the player leaves.
void CarGodFlagTick(const Settings& A, void* veh) {
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

// ---- vehicle extras ------------------------------------------------------------------
void VehicleExtrasTick(const Settings& A, void* me) {
    void* veh = PlayerVehicle();
    // rainbow paint
    if (A.rainbowCar && veh) {
        static DWORD next = 0;
        DWORD now = GetTickCount();
        if (now >= next) {
            next = now + 700;                                          // a new colour every 0.7 s
            int n = (int)CarPalette().size(); if (n < 2) n = 95;
            int c1 = (at<unsigned char>(veh, VEH_COLOR1) + 1) % n;
            at<unsigned char>(veh, VEH_COLOR1) = (unsigned char)c1;
            at<unsigned char>(veh, VEH_COLOR2) = (unsigned char)((c1 + n / 2) % n);
        }
    }
    // bulletproof tyres: SET_CAN_BURST_CAR_TYRES (053F) off for the car you're in
    static void* tyresDone = nullptr;
    if (veh && veh != tyresDone && A.bulletproofTyres) {
        int t = at<int>(veh, VEH_TYPE);
        if (t == 0 || t == 5) Scm(0x53F).I(CarH(veh)).I(0).Run();
        tyresDone = veh;
    } else if (tyresDone && (!A.bulletproofTyres || veh != tyresDone)) {
        if (IsLiveVehicle(tyresDone) && !A.bulletproofTyres) Scm(0x53F).I(CarH(tyresDone)).I(1).Run();
        tyresDone = nullptr;
    }
    // performance: acceleration, grip and brakes, on top of the car's own handling
    if (veh && at<void*>(veh, VEH_DRIVER) == me &&
        (fabsf(A.carAccel - 1.0f) > 0.01f || fabsf(A.carGrip - 1.0f) > 0.01f || fabsf(A.carBrakes - 1.0f) > 0.01f)) {
        int t = at<int>(veh, VEH_TYPE);
        float* mv = &at<float>(veh, PHYS_MOVE);
        const float* f = &at<float>(veh, ENT_FWD);
        float ts = fminf(*TimeStep, 3.0f);
        bool onGround = fabsf(mv[2]) < 0.06f;
        if ((t == 0 || t == 5 || t == 1) && onGround && !g_menuOpen) {
            float along = mv[0] * f[0] + mv[1] * f[1] + mv[2] * f[2];
            bool gas = at<short>(Pad0, 0x20) != 0, brake = at<short>(Pad0, 0x1C) != 0;
            if (gas && A.carAccel > 1.0f && along > -0.01f)
                for (int i = 0; i < 3; i++) mv[i] += f[i] * 0.003f * (A.carAccel - 1.0f) * ts;
            if (brake && A.carBrakes > 1.0f && along > 0.02f) {
                float cut = fminf(along - 0.01f, 0.004f * (A.carBrakes - 1.0f) * ts);
                for (int i = 0; i < 3; i++) mv[i] -= f[i] * cut;
            }
            if (A.carGrip > 1.0f && t != 1) {                              // less sideways slide
                float sx = mv[0] - f[0] * along, sy = mv[1] - f[1] * along;
                float k = fminf(0.05f * (A.carGrip - 1.0f) * ts, 0.9f);
                mv[0] -= sx * k; mv[1] -= sy * k;
            }
        }
    }
}

void DeleteVehicle() {
    void* me = PlayerPed(); if (!me) return;
    void* veh = PlayerVehicle();
    if (veh) {
        WarpPlayerOut(veh, me);
        DeleteCarWithPeople(veh, me);
        ShowNative("Vehicle deleted");
        return;
    }
    // on foot: the nearest one within 10 m
    char* pool = (char*)*VehiclePool; if (!pool) return;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* fl = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    const float* mp = &at<float>(me, ENT_POS);
    void* best = nullptr; float bestD = 10.0f;
    for (int i = 0; i < size; i++) {
        if (fl[i] & 0x80) continue;
        void* v = objs + i * VEHICLE_POOL_STRIDE;
        const float* p = &at<float>(v, ENT_POS);
        float d = sqrtf((p[0] - mp[0]) * (p[0] - mp[0]) + (p[1] - mp[1]) * (p[1] - mp[1]) + (p[2] - mp[2]) * (p[2] - mp[2]));
        if (d < bestD) { bestD = d; best = v; }
    }
    if (!best) { ShowNative("No vehicle within 10 m"); return; }
    DeleteCarWithPeople(best, me);
    ShowNative("Vehicle deleted");
}

// ---- garage: saved vehicles ----------------------------------------------------------
static const char* kGarageFile = "VCTrainer_garage.txt";
std::vector<GarageCar>& Garage() {
    static std::vector<GarageCar> list;
    static bool loaded = false;
    if (loaded) return list;
    loaded = true;
    if (FILE* f = OpenGameFile(kGarageFile, "r", false)) {
        char line[256];
        while (fgets(line, sizeof line, f)) {
            GarageCar g{}; char name[160] = {};
            if (sscanf(line, "%d %d %d %159[^\r\n]", &g.model, &g.c1, &g.c2, name) >= 3) { g.name = name; list.push_back(g); }
        }
        fclose(f);
    }
    return list;
}
static void GarageWrite() {
    if (FILE* f = OpenGameFile(kGarageFile, "w")) {
        for (auto& g : Garage()) fprintf(f, "%d %d %d %s\n", g.model, g.c1, g.c2, g.name.c_str());
        fclose(f);
    } else ShowNative("Couldn't save the garage - see VCTrainer.log");
}
void GarageSave(const char* name) {
    void* v = PlayerVehicle();
    if (!v) { ShowNative("Get in the vehicle you want to save"); return; }
    GarageCar g{ at<short>(v, ENT_MODEL), at<unsigned char>(v, VEH_COLOR1), at<unsigned char>(v, VEH_COLOR2), name && *name ? name : "" };
    if (g.name.empty())
        for (auto& e : VehicleList()) if (e.model == g.model) { g.name = e.name; break; }
    if (g.name.empty()) g.name = "Vehicle " + std::to_string(g.model);
    Garage().push_back(g);
    GarageWrite();
    ShowNative("Saved to the garage: %s", g.name.c_str());
}
void GarageSpawn(int i) {
    auto& l = Garage();
    if (i < 0 || i >= (int)l.size()) return;
    if (void* v = SpawnVehicleObj(l[i].model, true)) {
        at<unsigned char>(v, VEH_COLOR1) = (unsigned char)l[i].c1;
        at<unsigned char>(v, VEH_COLOR2) = (unsigned char)l[i].c2;
    }
}
void GarageRemove(int i) {
    auto& l = Garage();
    if (i < 0 || i >= (int)l.size()) return;
    l.erase(l.begin() + i);
    GarageWrite();
}

// Script opcode 02EB (restore_camera_jumpcut): the camera re-picks its mode
// and target from what the player is in, at once.
static void RestoreCameraJumpCut() { Scm(0x2EB).Run(); }
