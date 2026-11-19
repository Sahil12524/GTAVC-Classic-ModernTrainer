// Combat: kill peds by touch, aim lock and silent aim, wall bang / one-shot
// vehicles / bullet penetration, the drive-by aimbot, genocide, rapid fire,
// no-scope and full-auto sniper, the enemy markers, the Hunter aimbot, special
// bullets and guns, and super punch.
#include "features_internal.h"

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

// A ped seated in a vehicle or getting in / out of one (PED_DRIVING 50,
// PASSENGER 51, TAXI_PASSENGER 52, OPEN_DOOR 53, CARJACK 56, DRAG_FROM_CAR 57,
// ENTER_CAR 58, STEAL_CAR 59, EXIT_CAR 60). Its collision (+0x51 bit 0) must
// never be switched on: it then collides with its own car, whose physics blow
// up (absurd heading) and the AI steering's angle-wrap loop spins forever.
static bool InOrBoardingVehicle(void* p) {
    if (at<unsigned char>(p, PED_IN_VEHICLE)) return true;
    int st = at<int>(p, PED_STATE);
    return (st >= 50 && st <= 53) || (st >= 56 && st <= 60);
}

void TouchKillTick(const Settings& A) {
    // Touched peds the game hasn't killed yet: hit again every frame for up
    // to 1 s. CPed::InflictDamage can refuse a ped for a moment (knocked
    // down by the hit, collision briefly off), and health 0 alone doesn't
    // make a ped die - before, such a ped was left standing at 0 health and
    // skipped from then on.
    static std::vector<std::pair<void*, DWORD>> pending;
    DWORD now = GetTickCount();
    void* me = PlayerPed();
    void* veh = me ? PlayerVehicle() : nullptr;
    if (!A.touchKill || !veh) { pending.clear(); return; }      // vehicle feature only
    char* pool = (char*)*PedPool; if (!pool) return;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    const float* p = &at<float>(veh, ENT_POS);
    const float* r = &at<float>(veh, ENT_RIGHT);
    const float* f = &at<float>(veh, ENT_FWD);
    const float* u = &at<float>(veh, ENT_UP);
    const float* mv = &at<float>(veh, PHYS_MOVE);

    // The vehicle's own collision box (bus, truck, bike, boat...), not a
    // fixed car size, plus a ped's width around it.
    float mn[3] = { -1.1f, -2.6f, -1.0f }, mx[3] = { 1.1f, 2.6f, 1.5f };
    short model = at<short>(veh, ENT_MODEL);
    if (model >= 0 && model < 6500) {
        char* mi = (char*)ModelInfoPtrs[model];
        char* col = mi && !IsBadReadPtr(mi, MI_COLMODEL + 4) ? *(char**)(mi + MI_COLMODEL) : nullptr;
        if (col && !IsBadReadPtr(col, COL_BOX_MAX + 12)) {
            const float* bmn = (const float*)(col + COL_BOX_MIN);
            const float* bmx = (const float*)(col + COL_BOX_MAX);
            if (bmx[0] - bmn[0] > 0.2f && bmx[0] - bmn[0] < 25.0f && bmx[1] - bmn[1] > 0.4f && bmx[1] - bmn[1] < 50.0f)
                for (int k = 0; k < 3; k++) { mn[k] = bmn[k]; mx[k] = bmx[k]; }
        }
    }
    const float M = 0.45f;                                          // a ped's radius
    // Fast: the vehicle moves up to ~1 m between frames - stretch the box
    // forward / back by that much so a ped can't slip through between two.
    float along = mv[0] * f[0] + mv[1] * f[1] + mv[2] * f[2];      // per 1/50 s step
    float sweep = fminf(fabsf(along) * 1.5f, 3.0f);
    float front = mx[1] + M + (along > 0 ? sweep : 0.0f), back = mn[1] - M - (along < 0 ? sweep : 0.0f);

    auto kill = [&](void* o) {
        // A ped opening a door / jacking has collision switched off for the
        // animation, and InflictDamage refuses peds without collision.
        // Peds boarding / seated keep their collision as it is (see
        // InOrBoardingVehicle); the armour / health write below kills them.
        if (!InflictDamage(o, me, 45 /* UNIDENTIFIED */, 1000.0f, 0, 0) && !InOrBoardingVehicle(o)) {
            unsigned char was = at<unsigned char>(o, 0x51);
            at<unsigned char>(o, 0x51) = was | 1;                  // bUsesCollision, for this call only
            InflictDamage(o, me, 45 /* UNIDENTIFIED */, 1000.0f, 0, 0);
            at<unsigned char>(o, 0x51) = (at<unsigned char>(o, 0x51) & ~1) | (was & 1);
        }
        at<float>(o, PED_ARMOUR) = 0.0f;
        at<float>(o, PED_HEALTH) = 0.0f;
    };
    auto dying = [](void* o) { int st = at<int>(o, PED_STATE); return st == PED_DIE || st == PED_DEAD; };

    for (int i = 0; i < size; i++) {
        if (flags[i] & 0x80) continue;
        void* o = objs + i * PED_POOL_STRIDE;
        if (o == me || dying(o)) continue;
        if (at<unsigned char>(o, PED_IN_VEHICLE)) continue;               // sitting in a vehicle
        // Getting into ours: only the player's own people are spared (riding
        // with or following the player). Anyone else at the door - a carjacker,
        // a vigilante / mission target going for the driver's seat - dies on touch.
        if (at<void*>(o, PED_MY_VEHICLE) == veh) {
            bool seated = false;
            void** seats = &at<void*>(veh, VEH_DRIVER);
            for (int k = 0; k < 9; k++) if (seats[k] == o) seated = true;
            bool friendOf = at<int>(o, PED_OBJECTIVE) == OBJ_FOLLOW_CHAR_IN_FORMATION && at<void*>(o, PED_PED_IN_OBJECTIVE) == me;
            bool passenger = at<int>(o, PED_OBJECTIVE) == 0x12;           // ENTER_CAR_AS_PASSENGER: taxi fares, partners
            if (seated || friendOf || passenger) continue;
        }
        const float* op = &at<float>(o, ENT_POS);
        float d[3] = { op[0] - p[0], op[1] - p[1], op[2] - p[2] };
        if (fabsf(d[0]) > 30.0f || fabsf(d[1]) > 30.0f || fabsf(d[2]) > 30.0f) continue;
        bool hit = InCollisionRecords(veh, o) || InCollisionRecords(o, veh);
        if (!hit) {
            float lx = d[0] * r[0] + d[1] * r[1] + d[2] * r[2];
            float ly = d[0] * f[0] + d[1] * f[1] + d[2] * f[2];
            float lz = d[0] * u[0] + d[1] * u[1] + d[2] * u[2];
            // the ped's position is its middle: body from ~1 m below to ~0.8 m above
            hit = lx >= mn[0] - M && lx <= mx[0] + M && ly >= back && ly <= front &&
                  lz >= mn[2] - 0.9f && lz <= mx[2] + 1.0f;
        }
        if (!hit) continue;
        kill(o);
        bool listed = false;
        for (auto& q : pending) if (q.first == o) listed = true;
        if (!listed) pending.push_back({ o, now });
    }
    // keep hitting the ones that didn't go down yet
    std::vector<std::pair<void*, DWORD>> keep;
    for (auto& q : pending) {
        if (now - q.second > 1000 || !IsLivePed(q.first) || dying(q.first)) continue;
        kill(q.first);
        keep.push_back(q);
    }
    pending.swap(keep);
}

bool IsLivePed(void* p) {
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
// Where to shoot a ped: the chest when standing, the upper body when crouched
// (the ped's position stays at standing height, the body drops ~0.6 m), the
// body on the ground when knocked down (falling / lying / getting up) so the
// lock follows them down.
static bool PedDucking(void* p) { return (at<unsigned char>(p, PED_FLAGS_150) & PED_DUCKING_BIT) != 0; }

// The world matrix of one of a ped's bones (game.h, PED_FRAMES): the same
// matrix CPed's attack code takes the muzzle from, and the one the ped's hit
// spheres are built on. The matrices are rebuilt from the current pose and
// position first (CEntity::UpdateRpHAnim), once per ped per game tick, so a
// ped that hasn't been drawn since it moved still gives where it is now.
// nullptr: no skinned clump / hierarchy / frame for that node yet (a ped not
// set up for drawing) - the caller falls back to its old formula.
static int g_boneReads = 0, g_boneFallbacks = 0;
static const float* PedBoneMatrix(void* p, int node, const char** why) {
    void* clump = at<void*>(p, ENT_CLUMP);
    if (!clump || *(unsigned char*)clump != 2) { *why = "no clump"; return nullptr; }   // rpCLUMP
    if (!IsClumpSkinned(clump)) { *why = "clump not skinned"; return nullptr; }
    char* hier = SkinHierarchy(clump);
    if (!hier) { *why = "no bone hierarchy"; return nullptr; }
    int n = at<int>(hier, HIER_NUM_NODES);
    char* mats = at<char*>(hier, HIER_MATRICES);
    if (n <= 0 || n > 64 || !mats || !at<void*>(hier, HIER_CURRENT_ANIM)) { *why = "hierarchy not set up"; return nullptr; }
    char* fd = at<char*>(p, PED_FRAMES + node * 4);
    if (!fd) { *why = "no frame data for the node"; return nullptr; }
    // once per ped per tick (CTimer ms moves on every tick)
    static unsigned stamp = 0;
    static void* done[160];
    static int nDone = 0;
    if (stamp != *TimeMs) { stamp = *TimeMs; nDone = 0; }
    bool fresh = false;
    for (int i = 0; i < nDone; i++) if (done[i] == p) { fresh = true; break; }
    if (!fresh) {
        EntityUpdateRpHAnim(p);
        if (nDone < 160) done[nDone++] = p;
    }
    int idx = HAnimIDGetIndex(hier, at<int>(fd, FRAMEDATA_NODE_ID));
    if (idx < 0 || idx >= n) { *why = "node not in the hierarchy"; return nullptr; }
    const float* m = (const float*)(mats + idx * 0x40);
    const float* pp = &at<float>(p, ENT_POS);
    float dx = m[12] - pp[0], dy = m[13] - pp[1], dz = m[14] - pp[2];
    if (!(dx * dx + dy * dy + dz * dz < 3.0f * 3.0f)) { *why = "bone far from the ped (stale)"; return nullptr; }
    return m;
}

static void LogAimPointSource(bool ok, const char* why) {
    static bool loggedOk = false;
    static const char* loggedWhy[8];
    static int nWhy = 0;
    if (ok) {
        if (!loggedOk) { loggedOk = true; Log("AimPoint: bone positions from frames (spine node %d, head node %d)", PED_NODE_MID, PED_NODE_HEAD); }
        return;
    }
    for (int i = 0; i < nWhy; i++) if (loggedWhy[i] == why) return;
    if (nWhy < 8) loggedWhy[nWhy++] = why;
    Log("AimPoint: fallback to the position formula (%s)", why);
}

// Where to shoot a ped: its spine (Spine1, mid chest) - or its head, for the
// sniper - from the bones, wherever the body is: standing, crouched, running,
// lying on the ground. No bones yet: the old formula (chest above the ped's
// position point; lower when crouched; just above the ground when down).
static void EnemyAimPoint(void* p, float* out, bool head = false) {
    const char* why = "";
    if (const float* m = PedBoneMatrix(p, head ? PED_NODE_HEAD : PED_NODE_MID, &why)) {
        out[0] = m[12]; out[1] = m[13]; out[2] = m[14];
        g_boneReads++;
        LogAimPointSource(true, nullptr);
        return;
    }
    g_boneFallbacks++;
    LogAimPointSource(false, why);
    const float* pp = &at<float>(p, ENT_POS);
    out[0] = pp[0]; out[1] = pp[1]; out[2] = pp[2] + (head ? 0.7f : 0.45f) - (PedDucking(p) ? 0.65f : 0.0f);
    int st = at<int>(p, PED_STATE);
    if (st == PED_FALL || st == PED_GETUP) {
        bool f = false;
        float g = FindGroundZ3D(pp[0], pp[1], pp[2] + 0.5f, &f);
        out[2] = f ? g + 0.25f : pp[2] - 0.3f;
    }
}

// The muzzle of the gun in hand: the weapon's fire offset (CWeaponInfo +0x28)
// through the right hand's bone matrix, as CPed's own attack does before
// CWeapon::Fire (0x52B86F..0x52B9B4); without bones, through the ped's matrix
// (the game's own melee path, 0x52B8EC).
static int g_muzzleBone = 0, g_muzzlePed = 0;
static void MuzzlePos(void* me, char* w, float* out) {
    float o[3] = { 0.0f, 0.0f, 0.0f };
    if (char* wi = GetWeaponInfo(at<int>(w, WEAP_TYPE))) memcpy(o, wi + WI_FIRE_OFFSET, 12);
    const char* why = "";
    if (const float* m = PedBoneMatrix(me, PED_NODE_HANDR, &why)) {
        for (int i = 0; i < 3; i++) out[i] = m[i] * o[0] + m[4 + i] * o[1] + m[8 + i] * o[2] + m[12 + i];
        g_muzzleBone++;
        return;
    }
    const float* r = &at<float>(me, ENT_RIGHT);
    const float* f = &at<float>(me, ENT_FWD);
    const float* u = &at<float>(me, ENT_UP);
    const float* p = &at<float>(me, ENT_POS);
    for (int i = 0; i < 3; i++) out[i] = r[i] * o[0] + f[i] * o[1] + u[i] * o[2] + p[i];
    g_muzzlePed++;
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

bool IsHostile(void* p, void* me) {
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

bool g_flyingOnFoot = false;   // fly mode on, on foot (set by FlyTick)

// Can a shot reach this ped? A clear line (fences / glass don't count) from
// the camera or from the player's own eyes to the ped's head, chest or legs.
static bool PedReachable(void* me, const float* camSrc, void* p) {
    const float* tp = &at<float>(p, ENT_POS);
    const float* mp = &at<float>(me, ENT_POS);
    float chest[3]; EnemyAimPoint(p, chest);                               // the bones: wherever the body is
    float head[3]; EnemyAimPoint(p, head, true);
    int st = at<int>(p, PED_STATE);
    bool down = st == PED_FALL || st == PED_GETUP;
    float legs[3] = { tp[0], tp[1], down ? chest[2] : tp[2] - 0.5f };
    float eye[3] = { mp[0], mp[1], mp[2] + 0.7f };
    auto clear = [](const float* a, const float* b) {
        return LineOfSightClear(a, b, true, false, false, false, false, true, true);
    };
    return clear(camSrc, chest) || clear(camSrc, head) || clear(eye, chest) || clear(eye, head) || clear(eye, legs);
}

static void* g_aimTarget = nullptr;   // who the aim lock is on (for the enemy markers)
bool g_bhopping = false;              // a bunny-hop chain is running (BhopTick)
static float g_aimError = 10.0f;      // angle between crosshair and that target, radians

// Weapons that normally need the right button held to shoot (a scope or the
// launcher's sight): sniper, laser sniper, rocket launcher.
bool IsScopeWeapon(int t) { return t == WEAPON_SNIPER || t == WEAPON_LASER_SNIPER || t == WEAPON_ROCKET_LAUNCHER; }
bool ScopedNow() {                                        // sniper / rocket camera up
    short m = at<short>(ActiveCam(), CAM_MODE);
    return m == 7 || m == 39 || m == 8 || m == 40;
}
// Any first-person weapon view (sniper, rocket sight, M4 / M60 first person,
// camera...): the views where the game reads the pad for zoom, not jumping.
bool FirstPersonWeaponCam() {
    short mode = at<short>(ActiveCam(), CAM_MODE);
    return mode == 7 || mode == 8 || mode == 16 || mode == 34 || (mode >= 39 && mode <= 42) || mode == 45 || mode == 46;
}

// Every shot the player fires goes through CWeapon::Fire (hooked in
// hooks.cpp), whatever the gun: pistols, SMGs, shotguns, rifles, rockets.
// Each one bumps this counter, which ShotTick below reads once a frame.
static volatile int g_playerShots = 0;
static bool g_trainerFiring = false;      // the trainer's own CWeapon::Fire call is running

// The Hunter / Sea Sparrow fire with a temporary CWeapon straight into
// FireProjectile / FireInstantHit (never CWeapon::Fire): hooked at those four
// calls (hooks.cpp), counted here and in g_playerShots, with where the shot left.
static volatile int g_heliShots = 0;
static float g_heliShotSrc[3];
static int g_heliShotType = -1;
static bool IsHunterModel(void* v) {
    if (!v) return false;
    short m = at<short>(v, ENT_MODEL);
    return m == 155 || m == 177;                                       // Hunter, Sea Sparrow
}
void TrainerHeliShot(void* w, void* shooter, const float* source, bool ok) {
    if (!ok || !w || !shooter || shooter != PlayerVehicle()) return;
    if (at<void*>(shooter, VEH_DRIVER) != FindPlayerPed()) return;     // the player flies it
    if (source) memcpy(g_heliShotSrc, source, sizeof g_heliShotSrc);
    else memcpy(g_heliShotSrc, &at<float>(shooter, ENT_POS), sizeof g_heliShotSrc);
    g_heliShotType = at<int>(w, WEAP_TYPE);
    g_heliShots = g_heliShots + 1;
    g_playerShots = g_playerShots + 1;                                 // ShotTick: wall bang, one-shot vehicles, penetration
}

// The game's drive-by shot this tick (FireFromCar hooks, hooks.cpp): the gun,
// the vehicle and the side DoDriveByShootings chose, and the game time it ran.
struct DriveByShot { void* w; void* veh; int left, right; unsigned at; bool valid; };
static DriveByShot g_dbShot{};
static volatile int g_driveByExtraShots = 0;                          // rapid fire's added drive-by shots
void TrainerDriveByShot(void* w, void* veh, int left, int right) {
    if (!w || !veh || veh != PlayerVehicle() || at<void*>(veh, VEH_DRIVER) != FindPlayerPed()) return;
    g_dbShot = DriveByShot{ w, veh, left, right, *TimeMs, true };
}
static DWORD g_gameShotAt = 0;            // when the game itself last fired the player's gun

// CWeapon::Fire for any view, from the muzzle (the trainer's extra shots used
// to pass no source, so the game started them at the player's position + 0.6:
// the flash, tracer and sound came from the chest / head). The player's rocket
// launcher only fires with the camera in a scope / sight mode, so it gets
// rocket-sight mode for the call.
bool FireAnyView(char* w, void* me) {
    float src[3];
    MuzzlePos(me, w, src);
    g_trainerFiring = true;
    bool ok;
    if (at<int>(w, WEAP_TYPE) != WEAPON_ROCKET_LAUNCHER || ScopedNow()) ok = WeaponFire(w, me, src);
    else {
        short* mode = &at<short>(ActiveCam(), CAM_MODE);
        short oldMode = *mode;
        *mode = 8;
        ok = WeaponFire(w, me, src);
        *mode = oldMode;
    }
    g_trainerFiring = false;
    return ok;
}

// ---- aim lock statistics (VCTrainer.log, every 60 s) ---------------------------------
// Every counted shot while the camera lock is on a ped: the angle between the
// crosshair and the target's bone at that tick, and whether the target lost
// health (or died) by the next tick.
static struct AimStats {
    int shots = 0, hit = 0, miss = 0;
    double errSum = 0.0; float errMax = 0.0f;
    int releases = 0, leadOn = 0, leadOff = 0;
    double settleSum = 0.0; float settleMax = 0.0f; int settleN = 0;
    int rapidFired = 0, rapidSkipState = 0, rapidSkipHeading = 0;
    // fire-time aim (TrainerWeaponFire): the crosshair line the bullet really
    // used against the locked ped's bone at that instant, before correction
    int fireN = 0, fireCorrected = 0, fireTooFar = 0, fireNotApplied = 0;
    double fireErrSum = 0.0; float fireErrMax = 0.0f;
    // direct kills and lock drops
    int seatedKills = 0, seatedDied = 0, seatedStuck = 0, seatedRetries = 0, downKills = 0, dropNoDamage = 0, dropUnderMap = 0;
    int collForced = 0, collRestored = 0;
    int acqN = 0, acqSum = 0, acqMax = 0;              // ticks from lock-on to crosshair on target
    int fireSniper = 0;                                // fire-time aim on the scoped sniper's bullet
} g_as;
// A shot is counted on the tick after it went off (the game's own shots during
// the tick's update, the trainer's extra ones after the aim lock ran), and an
// instant-hit bullet does its damage at once - so the target's health is
// compared with what it had the tick before (hp). No drop yet: one more tick.
struct PendingShot { void* t; float hp; };
static std::vector<PendingShot> g_pendingShots;
static float PedHp(void* p) { return at<float>(p, PED_HEALTH) + at<float>(p, PED_ARMOUR); }
static bool ShotLanded(void* t, float hpBefore) {
    if (!IsLivePed(t)) return true;                                        // gone: killed and removed
    int st = at<int>(t, PED_STATE);
    return st == PED_DIE || st == PED_DEAD || PedHp(t) < hpBefore - 0.01f;
}
static void RecordShot(void* t, float hpBefore, float err) {
    g_as.shots++;
    g_as.errSum += err;
    if (err > g_as.errMax) g_as.errMax = err;
    if (ShotLanded(t, hpBefore)) g_as.hit++;
    else g_pendingShots.push_back({ t, hpBefore });
}

// The trainer's own direct damage (silent aim, wall bang, no-scope sniper,
// drive-by): CPed::InflictDamage's answer for the last ped it was called on,
// for the stuck-target diagnostic in AimLockTick.
static void* g_dmgPed = nullptr;
static int g_dmgCalls = 0, g_dmgRefused = 0, g_dmgLast = -1;      // -1: never called on it
static bool TrainerDamage(void* p, void* me, int type, float damage) {
    bool r = InflictDamage(p, me, type, damage, 0, 0);
    if (p != g_dmgPed) { g_dmgPed = p; g_dmgCalls = g_dmgRefused = 0; }
    g_dmgCalls++;
    if (!r) g_dmgRefused++;
    g_dmgLast = r ? 1 : 0;
    return r;
}

// Direct damage that never leaves a ped's collision changed. CPed::InflictDamage
// refuses a ped without collision (0x525C50) unless it is driving (state 50)
// or the weapon is 43. A ped on foot without it gets the bit for this one
// call and its old value back right after; a ped seated in or boarding a
// vehicle is never given it (InOrBoardingVehicle: it would collide with its
// own car). Its return value is not "refused": it can be false with the
// damage done (VCTrainer.log: health 68 -> 2, false), so callers look at health.
static bool SafeDamage(void* p, void* me, int type, float damage) {
    unsigned char was = at<unsigned char>(p, ENT_FLAGS_51);
    if ((was & 1) || InOrBoardingVehicle(p)) return TrainerDamage(p, me, type, damage);
    at<unsigned char>(p, ENT_FLAGS_51) = was | 1;
    g_as.collForced++;
    bool r = TrainerDamage(p, me, type, damage);
    at<unsigned char>(p, ENT_FLAGS_51) = (at<unsigned char>(p, ENT_FLAGS_51) & ~1) | (was & 1);
    g_as.collRestored++;
    return r;
}
static bool PedDown(void* p) { int st = at<int>(p, PED_STATE); return st == PED_FALL || st == PED_GETUP; }
static bool PedGone(void* p) {
    if (!IsLivePed(p)) return true;
    int st = at<int>(p, PED_STATE);
    return st == PED_DIE || st == PED_DEAD;
}
// Killed outright, as TouchKillTick does: weapon 45 for 1000, then no armour
// and no health (collision handled as SafeDamage does).
static void KillOutright(void* p, void* me) {
    SafeDamage(p, me, 45 /* UNIDENTIFIED */, 1000.0f);
    at<float>(p, PED_ARMOUR) = 0.0f;
    at<float>(p, PED_HEALTH) = 0.0f;
}
// A seated ped killed outright doesn't always go down on the same tick. It
// is followed for up to 2 s and killed again each tick it is still alive -
// only the ped: its car is never damaged or blown up by this (an earlier
// version blew the car up after 3 ticks; 48 of 81 seated kills had died
// within those 3 ticks on their own, VCTrainer.log 2026-11-19). The outcome is
// logged: died after N ticks, or still alive after 2 s.
static void* g_seatedKill = nullptr;
static int g_seatedKillTicks = 0;
static void SeatedKillLog(const char* fmt, void* p, int ticks) {
    static int logs = 0;
    if (logs++ >= 200) return;
    void* v = at<void*>(p, PED_MY_VEHICLE);
    Log(fmt, (unsigned)(DWORD)p, at<short>(p, ENT_MODEL), ticks, at<int>(p, PED_STATE), at<float>(p, PED_HEALTH),
        at<unsigned char>(p, PED_IN_VEHICLE), (unsigned)(DWORD)v, v && IsLiveVehicle(v) ? at<float>(v, VEH_HEALTH) : -1.0f);
}
static void SeatedKillTick(void* me) {
    if (!g_seatedKill) return;
    void* p = g_seatedKill;
    g_seatedKillTicks++;
    if (PedGone(p)) {
        g_as.seatedDied++;
        if (IsLivePed(p))
            SeatedKillLog("AimLock: seated kill: ped 0x%08X model %d died after %d ticks (state %d health %.0f inVehicle %d car 0x%08X health %.0f, car untouched)",
                          p, g_seatedKillTicks);
        g_seatedKill = nullptr;
        return;
    }
    if (g_seatedKillTicks > 60) {                                          // 2 s at 30 ticks a second
        g_as.seatedStuck++;
        SeatedKillLog("AimLock: seated kill: ped 0x%08X model %d still alive after %d ticks (state %d health %.0f inVehicle %d car 0x%08X health %.0f) - left alone, car untouched",
                      p, g_seatedKillTicks);
        g_seatedKill = nullptr;
        return;
    }
    KillOutright(p, me);                                                   // the ped only, no collision write while seated
    g_as.seatedRetries++;
}
static void StartSeatedKill(void* p, void* me, const char* path) {
    float carHealth = -1.0f;
    void* v = at<void*>(p, PED_MY_VEHICLE);
    if (v && IsLiveVehicle(v)) carHealth = at<float>(v, VEH_HEALTH);
    KillOutright(p, me);
    g_as.seatedKills++;
    g_seatedKill = p; g_seatedKillTicks = 0;
    static int logs = 0;
    if (logs++ < 200)
        Log("AimLock: seated kill via %s: ped 0x%08X model %d state %d inVehicle %d collision %d car 0x%08X health %.0f -> %.0f "
            "(InflictDamage weapon 45 x1000, health/armour 0; no collision write, no car damage)",
            path, (unsigned)(DWORD)p, at<short>(p, ENT_MODEL), at<int>(p, PED_STATE), at<unsigned char>(p, PED_IN_VEHICLE),
            at<unsigned char>(p, ENT_FLAGS_51) & 1, (unsigned)(DWORD)v, carHealth,
            v && IsLiveVehicle(v) ? at<float>(v, VEH_HEALTH) : -1.0f);
}
// Weapon damage on a ped; one that shows no health drop while down (falling /
// getting up) or seated / boarding a vehicle is killed outright instead.
static void DamageOrKill(void* p, void* me, int type, float damage) {
    float hp0 = PedHp(p);
    SafeDamage(p, me, type, damage);
    if (PedGone(p) || PedHp(p) < hp0 - 0.01f) return;
    bool seated = InOrBoardingVehicle(p);
    if (!seated && !PedDown(p)) return;
    if (seated) StartSeatedKill(p, me, "direct damage (silent aim / wall bang / no-scope)");
    else { KillOutright(p, me); g_as.downKills++; }
}

static void AimStatsTick() {
    for (auto& s : g_pendingShots) { if (ShotLanded(s.t, s.hp)) g_as.hit++; else g_as.miss++; }
    g_pendingShots.clear();
    static DWORD next = 0;
    static int lastShots = -1, lastSkips = -1;
    DWORD now = GetTickCount();
    if (!next) next = now + 60000;
    if (now < next) return;
    next = now + 60000;
    int skips = g_as.rapidSkipState + g_as.rapidSkipHeading;
    static int lastFire = -1;
    if (g_as.shots == lastShots && skips == lastSkips && g_as.fireN == lastFire) return;   // nothing new: no line
    lastShots = g_as.shots; lastSkips = skips; lastFire = g_as.fireN;
    Log("AimLock: %d shots, %d hit, %d missed (avg error %.1f deg, max %.1f); lead on %d ticks, off %d; "
        "camera reached the written turn within %.2f deg avg (max %.2f) a tick later; %d mouse releases; "
        "aim points from bones %d, fallback %d",
        g_as.shots, g_as.hit, g_as.miss, g_as.shots ? g_as.errSum / g_as.shots * 57.29578 : 0.0, g_as.errMax * 57.29578f,
        g_as.leadOn, g_as.leadOff, g_as.settleN ? g_as.settleSum / g_as.settleN * 57.29578 : 0.0, g_as.settleMax * 57.29578f,
        g_as.releases, g_boneReads, g_boneFallbacks);
    Log("AimLock: snap onto new target in %.1f ticks avg (max %d) over %d targets; fire-time aim on scoped sniper bullets %d",
        g_as.acqN ? (double)g_as.acqSum / g_as.acqN : 0.0, g_as.acqMax, g_as.acqN, g_as.fireSniper);
    Log("AimLock: fire-time aim %d shots, error before correction %.2f deg avg (max %.1f); corrected %d, too far %d, "
        "not on the mouse crosshair %d; killed seated %d (died %d, still alive after 2 s %d, re-kills %d; cars never touched), down %d; "
        "dropped no damage %d, under the map %d; collision forced %d / restored %d",
        g_as.fireN, g_as.fireN ? g_as.fireErrSum / g_as.fireN * 57.29578 : 0.0, g_as.fireErrMax * 57.29578f,
        g_as.fireCorrected, g_as.fireTooFar, g_as.fireNotApplied, g_as.seatedKills, g_as.seatedDied, g_as.seatedStuck,
        g_as.seatedRetries, g_as.downKills,
        g_as.dropNoDamage, g_as.dropUnderMap, g_as.collForced, g_as.collRestored);
    Log("RapidFire: %d extra shots, %d slots skipped (not attacking %d, not facing the camera %d); muzzle from hand bone %d, ped matrix %d",
        g_as.rapidFired, skips, g_as.rapidSkipState, g_as.rapidSkipHeading, g_muzzleBone, g_muzzlePed);
}

// ---- the aim locks' camera turns ----------------------------------------------------
// The locks decide their turn in TrainerTick, which runs after CGame::Process -
// after the game built this tick's camera. Written there, a turn showed up a
// tick late and the frames drawn in between saw a Beta / Alpha that didn't
// match the camera they drew. So the turn is queued and put in by the pad
// update hook at the start of the next CGame::Process (HookedUpdatePads,
// before the camera update), or by the lock itself next tick if that hook is
// missing. Alpha is clamped to the on-foot mouse camera's limits (game.h
// CAM_PITCH_MAX / MIN) in that mode, and what the camera really turned is
// what the snap-back later takes away.
struct CamTurn {
    char* cam = nullptr; float dBeta = 0.0f, dAlpha = 0.0f; bool valid = false, back = false;
    float turnedBeta = 0.0f, turnedAlpha = 0.0f;   // turned so far (the snap-back undoes it)
    char* turnedOn = nullptr;                       // the camera it was turned on
    bool resetOnCamChange = false;                  // drive-by: look left / right is another camera
};
static CamTurn g_lockTurn, g_driveByTurn;
static void ApplyTurn(CamTurn& t) {
    if (!t.valid) return;
    t.valid = false;
    char* cam = ActiveCam();
    if (cam != t.cam) { if (t.back) { t.turnedBeta = t.turnedAlpha = 0.0f; t.turnedOn = nullptr; } return; }   // camera changed meanwhile
    if (t.resetOnCamChange && !t.back && cam != t.turnedOn) { t.turnedBeta = t.turnedAlpha = 0.0f; }
    float a0 = at<float>(cam, CAM_ALPHA), a1 = a0 + t.dAlpha;
    if (at<short>(cam, CAM_MODE) == CAM_MODE_FOLLOWPED) a1 = fminf(CAM_PITCH_MAX, fmaxf(CAM_PITCH_MIN, a1));
    at<float>(cam, CAM_BETA) += t.dBeta;
    at<float>(cam, CAM_ALPHA) = a1;
    if (t.back) { t.turnedBeta = t.turnedAlpha = 0.0f; t.turnedOn = nullptr; }
    else { t.turnedBeta += t.dBeta; t.turnedAlpha += a1 - a0; t.turnedOn = cam; }
}
void ApplyPendingCameraTurns() { ApplyTurn(g_lockTurn); ApplyTurn(g_driveByTurn); }
static void QueueTurn(CamTurn& t, char* cam, float dBeta, float dAlpha) {
    t.cam = cam; t.dBeta = dBeta; t.dAlpha = dAlpha; t.back = false; t.valid = true;
}
static void ClearTurn(CamTurn& t) { t.valid = false; t.turnedBeta = t.turnedAlpha = 0.0f; t.turnedOn = nullptr; }
static void QueueTurnBack(CamTurn& t) {                                   // back by what was turned
    t.valid = false;
    if (t.turnedBeta == 0.0f && t.turnedAlpha == 0.0f) return;
    char* cam = ActiveCam();
    if (t.resetOnCamChange && cam != t.turnedOn) { ClearTurn(t); return; }
    t.cam = cam; t.dBeta = -t.turnedBeta; t.dAlpha = -t.turnedAlpha; t.back = true; t.valid = true;
}
// Never more than 30 degrees in one tick (it only smooths the swing; the lock stays on).
static void CapTurn(float& dBeta, float& dAlpha) {
    float m = sqrtf(dBeta * dBeta + dAlpha * dAlpha);
    if (m > 0.5236f) { dBeta *= 0.5236f / m; dAlpha *= 0.5236f / m; }
}

// The scoped sniper's shot is a travelling bullet (game.h SNIPER_BULLET_SPEED):
// a sniper rifle in hand in a first-person weapon view.
static bool SniperBulletShot(void* me) {
    int slot = at<signed char>(me, PED_CUR_WEAPON);
    if (slot < 0 || slot >= WEAPON_SLOTS) return false;
    int t = at<int>((char*)me + PED_WEAPONS + slot * WEAPON_SIZE, WEAP_TYPE);
    return (t == WEAPON_SNIPER || t == WEAPON_LASER_SNIPER) && FirstPersonWeaponCam();
}
// The locked police heli's speed, averaged over ~0.25 s (it wobbles every
// tick as it hovers); kept by AimLockTick, read by FireTimeAim too.
static void* g_heliVOf = nullptr;
static float g_heliV[3];
static void HeliSpeedTick(void* h, float tick) {
    const float* hv = &at<float>(h, PHYS_MOVE);
    if (g_heliVOf != h) { g_heliVOf = h; memcpy(g_heliV, hv, 12); return; }
    float k = 1.0f - expf(-(tick / 50.0f) / 0.25f);
    for (int i = 0; i < 3; i++) g_heliV[i] += (hv[i] - g_heliV[i]) * k;
}

// A police helicopter is aimed at the middle of its collision box (the model's
// col model bounding box, as EntityFootprint reads it), not its origin.
static void HeliAimPoint(void* h, float* out) {
    const float* p = &at<float>(h, ENT_POS);
    memcpy(out, p, 12);
    short model = at<short>(h, ENT_MODEL);
    if (model < 0 || model >= 6500) return;
    char* mi = (char*)ModelInfoPtrs[model];
    char* col = mi && !IsBadReadPtr(mi, MI_COLMODEL + 4) ? *(char**)(mi + MI_COLMODEL) : nullptr;
    if (!col || IsBadReadPtr(col, COL_BOX_MAX + 12)) return;
    const float* mn = (const float*)(col + COL_BOX_MIN);
    const float* mx = (const float*)(col + COL_BOX_MAX);
    float c[3];
    for (int k = 0; k < 3; k++) {
        c[k] = (mn[k] + mx[k]) * 0.5f;
        if (!(fabsf(c[k]) < 20.0f)) return;                                // not a sane box
    }
    const float* r = &at<float>(h, ENT_RIGHT);
    const float* f = &at<float>(h, ENT_FWD);
    const float* u = &at<float>(h, ENT_UP);
    for (int k = 0; k < 3; k++) out[k] = p[k] + r[k] * c[0] + f[k] * c[1] + u[k] * c[2];
}

// A sniper rifle in hand, scope down, no-scope sniper on.
static bool NoScopeSniperInHand(const Settings& A, void* me) {
    if (!A.noScopeSniper || PlayerVehicle()) return false;
    int slot = at<signed char>(me, PED_CUR_WEAPON);
    if (slot < 0 || slot >= WEAPON_SLOTS) return false;
    int type = at<int>((char*)me + PED_WEAPONS + slot * WEAPON_SIZE, WEAP_TYPE);
    return IsScopeWeapon(type) && !ScopedNow();
}

// The mouse camera's turn that puts its crosshair ray (from S along c) through
// T, with the camera orbiting the pivot P (it turns round the player it looks
// at, not round itself). A far target needs the plain angle between them; a
// target a metre or two from the player needs much less or much more, since
// the ray passes off to the side of the pivot. Round first, in the horizontal
// plane; then up / down in the vertical plane of the ray, with T where the
// round turn takes it. A target nearer the pivot than the ray ever passes gets
// the turn that brings the ray as close as it goes. False: the ray is (nearly)
// vertical - the plain angles are used.
static bool PivotTurn(const float* S, const float* c, const float* P, const float* T, float& dh, float& dp) {
    float ch = sqrtf(c[0] * c[0] + c[1] * c[1]);
    if (ch < 1e-3f) return false;
    float u[2] = { c[0] / ch, c[1] / ch }, n[2] = { -u[1], u[0] };       // along the ray, to its left
    float lp = n[0] * (P[0] - S[0]) + n[1] * (P[1] - S[1]);               // the pivot's side offset from the ray
    float tp[2] = { T[0] - P[0], T[1] - P[1] };
    float r = sqrtf(tp[0] * tp[0] + tp[1] * tp[1]);
    if (r < 1e-3f) return false;
    float psi0 = atan2f(n[0] * tp[0] + n[1] * tp[1], u[0] * tp[0] + u[1] * tp[1]);
    float psi = asinf(fmaxf(-1.0f, fminf(1.0f, -lp / r)));
    dh = psi0 - psi;
    while (dh > 3.14159265f) dh -= 6.2831853f;
    while (dh < -3.14159265f) dh += 6.2831853f;
    // up / down: the target as the round turn leaves it, in the ray's plane
    float cs = cosf(-dh), sn = sinf(-dh);
    float tr[2] = { tp[0] * cs - tp[1] * sn, tp[0] * sn + tp[1] * cs };   // T - P turned by -dh
    float t2[2] = { u[0] * tr[0] + u[1] * tr[1], T[2] - P[2] };           // (along, up) from the pivot
    float s2[2] = { u[0] * (S[0] - P[0]) + u[1] * (S[1] - P[1]), S[2] - P[2] };
    float v[2] = { ch, c[2] }, vl = sqrtf(ch * ch + c[2] * c[2]);
    v[0] /= vl; v[1] /= vl;
    float nn[2] = { -v[1], v[0] };                                         // above the ray
    float lp2 = -(nn[0] * s2[0] + nn[1] * s2[1]);
    float r2 = sqrtf(t2[0] * t2[0] + t2[1] * t2[1]);
    if (r2 < 1e-3f) return false;
    float phi0 = atan2f(nn[0] * t2[0] + nn[1] * t2[1], v[0] * t2[0] + v[1] * t2[1]);
    float phi = asinf(fmaxf(-1.0f, fminf(1.0f, -lp2 / r2)));
    dp = phi0 - phi;
    return true;
}

void AimLockTick(const Settings& A, void* me) {
    static void* locked = nullptr;
    struct Pub { void*& l; ~Pub() { g_aimTarget = l; } } pub{ locked };
    static DWORD releasedUntil = 0;
    DWORD now = GetTickCount();
    // shots since the last tick (statistics), and what the target had before them
    AimStatsTick();
    SeatedKillTick(me);
    // Targets the lock gave up on (no damage after 5 on-target shots, or under
    // the map): not picked again for 5 s.
    static struct { void* p; DWORD until; } blocked[8] = {};
    auto isBlocked = [&](void* p) {
        for (auto& b : blocked) if (b.p == p && now < b.until) return true;
        return false;
    };
    auto block = [&](void* p) {
        int k = 0;
        for (int i = 1; i < 8; i++) if (blocked[i].until < blocked[k].until) k = i;
        blocked[k].p = p; blocked[k].until = now + 5000;
    };
    static int seenShots = 0;
    int newShots = g_playerShots - seenShots;
    seenShots = g_playerShots;
    if (newShots < 0 || newShots > 20) newShots = 0;
    static void* hpOf = nullptr;
    static float hpBefore = 0.0f;
    struct HpNote { void*& l; void*& of; float& hp; ~HpNote() {
        of = (l && IsLivePed(l)) ? l : nullptr;
        if (of) hp = PedHp(of);
    } } hpNote{ locked, hpOf, hpBefore };
    // the turn written last tick, to see where the camera got to by now
    static bool cmdValid = false;
    static float cmdYaw = 0.0f, cmdPitch = 0.0f;
    bool hadCmd = cmdValid;
    cmdValid = false;
    // right mouse button - or, with a no-scope sniper, the left one (right would scope in)
    // Silent aim (the setting, or while bunny hopping with its own setting on):
    // the camera is never turned - the target is still picked and ShotTick
    // lands each shot on it.
    bool silent = A.aimSilent || (g_bhopping && A.aimSilentBhop);
    bool aiming = NewMouseState[1] != 0 || (NewMouseState[0] != 0 && NoScopeSniperInHand(A, me)) ||
                  (NewMouseState[0] != 0 && (silent || g_bhopping));   // silent / hopping: the fire button alone
    g_aimError = 10.0f;
    // How far the lock has turned the camera since it first locked on. When
    // the aim button is let go, or nobody is left to lock onto, the camera
    // turns back by exactly that much: the view returns to where the player
    // was looking (plus any mouse moves of their own in between).
    // (g_lockTurn: queued, put in before the next camera update, clamped.)
    ApplyTurn(g_lockTurn);                                                 // the pad hook didn't run: now
    auto turnBack = [] { QueueTurnBack(g_lockTurn); };
    // police helicopter lock: ticks, the biggest turn, ticks out of the camera's reach
    static int heliTicks = 0, heliOutOfReach = 0;
    static float heliMaxTurn = 0.0f;
    static DWORD heliLogAt = 0;
    if (heliTicks && now - heliLogAt >= 60000) {
        Log("AimLock: heli lock %d ticks, max turn %.1f deg/tick, out of reach (above 45 deg) %d ticks",
            heliTicks, heliMaxTurn * 57.29578f, heliOutOfReach);
        heliTicks = heliOutOfReach = 0; heliMaxTurn = 0.0f; heliLogAt = now;
    }
    if (!heliTicks) heliLogAt = now;
    if (!A.aimLock || !aiming || PlayerVehicle() || g_menuOpen || *GameMenuActive || !GameFocused()) {
        if (A.aimLock && A.aimSnapBack && !aiming && !PlayerVehicle() && !g_menuOpen && !*GameMenuActive) turnBack();
        else ClearTurn(g_lockTurn);
        locked = nullptr; return;
    }
    float mx = *(float*)(NewMouseState + 8), my = *(float*)(NewMouseState + 0xC);
    if (now < releasedUntil) return;

    float src[3], c[3];
    if (!CrosshairRay(me, src, c)) return;
    // Where the shot's ray really starts: the camera (CCam Source) in the
    // third-person views - src is only the player's spot projected onto it.
    // Angles to a target are measured from here: from src, a target a metre
    // or two from the player was off by tens of degrees and the lock flicked.
    char* aimCam = ActiveCam();
    bool thirdPerson = !IsFirstPersonCamMode(at<short>(aimCam, CAM_MODE));
    float eye[3];
    memcpy(eye, thirdPerson ? (const float*)(aimCam + CAM_SOURCE) : src, 12);
    // The mouse camera turns round the player (the point it looks at), not
    // round itself: its pivot is where the Front line passes the player.
    float pivot[3];
    bool usePivot = at<short>(aimCam, CAM_MODE) == CAM_MODE_FOLLOWPED;
    if (usePivot) {
        const float* F = (const float*)(aimCam + CAM_FRONT);
        const float* mp = &at<float>(me, ENT_POS);
        float fh = F[0] * F[0] + F[1] * F[1];
        if (fh < 1e-4f) usePivot = false;
        else {
            float t = (F[0] * (mp[0] - eye[0]) + F[1] * (mp[1] - eye[1])) / fh;
            for (int i = 0; i < 3; i++) pivot[i] = eye[i] + F[i] * t;
        }
    }
    // where the camera got to, against the turn written last tick
    if (hadCmd && !silent) {
        float yaw = atan2f(c[1], c[0]), pitch = asinf(fmaxf(-1.0f, fminf(1.0f, c[2])));
        float ey = yaw - cmdYaw;
        while (ey > 3.14159265f) ey -= 6.2831853f;
        while (ey < -3.14159265f) ey += 6.2831853f;
        float e = sqrtf(ey * ey + (pitch - cmdPitch) * (pitch - cmdPitch));
        if (e < 0.5f) {                                                    // not a cut / the player's flick
            g_as.settleSum += e; g_as.settleN++;
            if (e > g_as.settleMax) g_as.settleMax = e;
        }
    }

    // the sniper rifles aim at the head, everything else at the chest
    bool headShot = false;
    int curType = -1;
    {
        int slot = at<signed char>(me, PED_CUR_WEAPON);
        if (slot >= 0 && slot < WEAPON_SLOTS) {
            curType = at<int>((char*)me + PED_WEAPONS + slot * WEAPON_SIZE, WEAP_TYPE);
            headShot = curType == WEAPON_SNIPER || curType == WEAPON_LASER_SNIPER;
        }
    }
    auto aimPoint = [headShot](void* p, float* out) {
        if (IsHeliTarget(p)) { HeliAimPoint(p, out); return; }             // helicopter: the middle of its box
        EnemyAimPoint(p, out, headShot);
    };
    auto visible = [&](const float* a) {
        return LineOfSightClear(src, a, true, false, false, false, false, true, true);   // fences, glass: see through
    };
    auto angleTo = [&](const float* a) {
        float d[3] = { a[0] - eye[0], a[1] - eye[1], a[2] - eye[2] };
        float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (l < 0.5f || l > 250.0f) return 10.0f;
        float dot = (d[0] * c[0] + d[1] * c[1] + d[2] * c[2]) / l;
        return acosf(fmaxf(-1.0f, fminf(1.0f, dot)));
    };

    // keep the current target while it is still a threat and in sight
    float ap[3];
    if (locked) {
        // police helicopters only with one-shot vehicles on
        bool ok = (A.oneShotVehicles && IsHeliTarget(locked)) || (IsLivePed(locked) && IsHostile(locked, me) && !isBlocked(locked));
        if (!ok) locked = nullptr;
        else {
            aimPoint(locked, ap);
            if (angleTo(ap) > 9.0f) locked = nullptr;                      // walls don't break it -
            else if (silent && !A.wallBang && !A.bulletPenetrate && !IsHeliTarget(locked) && !PedReachable(me, src, locked))
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
            if (!IsHostile(p, me) || isBlocked(p)) continue;
            float a[3]; aimPoint(p, a);
            float ang = angleTo(a);
            if (ang > 9.0f) continue;                                      // too near / too far
            float dist = sqrtf((a[0] - src[0]) * (a[0] - src[0]) + (a[1] - src[1]) * (a[1] - src[1]) + (a[2] - src[2]) * (a[2] - src[2]));
            float score;
            if (silent) {
                // silent aim: the one nearest your crosshair (a little for distance), and only
                // ones a shot can reach unless wall bang is on
                score = ang * 30.0f + dist * 0.05f;
                if (score < best && !A.wallBang && !A.bulletPenetrate && !PedReachable(me, src, p)) continue;
            } else {
                score = dist * (1.0f + ang * 0.5f);
                if (score < best && !visible(a)) score *= 2.5f;            // behind a wall: still a target, seen ones first
            }
            if (score < best) { best = score; locked = p; }
        }
        for (int i = 0; i < HELI_COUNT && A.oneShotVehicles; i++) {      // police helicopters too (one-shot vehicles on only)
            void* h = HeliList[i];
            if (!IsHeliTarget(h)) continue;
            float a[3]; aimPoint(h, a);
            float ang = angleTo(a);
            if (ang > 9.0f) continue;
            float dist = sqrtf((a[0] - src[0]) * (a[0] - src[0]) + (a[1] - src[1]) * (a[1] - src[1]) + (a[2] - src[2]) * (a[2] - src[2]));
            float score = silent ? ang * 30.0f + dist * 0.05f : dist * (1.0f + ang * 0.5f);
            if (score < best) { best = score; locked = h; }
        }
        if (!locked) { if (A.aimSnapBack) turnBack(); else ClearTurn(g_lockTurn); return; }                                       // nobody left: back to the player's view
        aimPoint(locked, ap);
    }

    // The turn from the crosshair to the target's bone (Beta: round, Alpha:
    // up / down), and the angle left between them (seen from the camera: 0
    // when the shot's ray goes through the bone).
    auto turnTo = [&](const float* a, float& dh, float& dp, float& err) {
        float d[3] = { a[0] - eye[0], a[1] - eye[1], a[2] - eye[2] };
        float dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (dl < 0.01f) { dh = dp = err = 0.0f; return; }
        for (float& v : d) v /= dl;
        err = acosf(fmaxf(-1.0f, fminf(1.0f, c[0] * d[0] + c[1] * d[1] + c[2] * d[2])));
        if (usePivot && PivotTurn(eye, c, pivot, a, dh, dp)) return;
        dh = atan2f(d[1], d[0]) - atan2f(c[1], c[0]);
        while (dh > 3.14159265f) dh -= 6.2831853f;
        while (dh < -3.14159265f) dh += 6.2831853f;
        dp = asinf(fmaxf(-1.0f, fminf(1.0f, d[2]))) - asinf(fmaxf(-1.0f, fminf(1.0f, c[2])));
    };
    float dh0, dp0, err0;
    turnTo(ap, dh0, dp0, err0);
    g_aimError = err0;
    // how many ticks from locking on until the crosshair is on it (within 1 degree)
    {
        static void* acqOf = nullptr;
        static int acqTicks = 0;
        static bool acqDone = true;
        if (locked != acqOf) { acqOf = locked; acqTicks = 0; acqDone = silent; }
        if (!acqDone) {
            if (err0 < 0.01745f) {
                g_as.acqN++; g_as.acqSum += acqTicks;
                if (acqTicks > g_as.acqMax) g_as.acqMax = acqTicks;
                acqDone = true;
            } else if (++acqTicks > 300) acqDone = true;                   // never got there (out of reach): not counted
        }
    }

    // Diagnostic (a ped under the road that never dies): locked over 1 s, the
    // player fired since the last tick and the target's health didn't drop -
    // what the ped is, once a second.
    {
        static void* lockOf = nullptr;
        static DWORD lockSince = 0, lastDiag = 0;
        static int diagLines = 0;
        if (locked != lockOf) { lockOf = locked; lockSince = now; }
        if (newShots > 0 && hpOf == locked && !IsHeliTarget(locked) && now - lockSince > 1000 &&
            now - lastDiag >= 1000 && diagLines < 300 && !ShotLanded(locked, hpBefore)) {
            lastDiag = now; diagLines++;
            const float* lp = &at<float>(locked, ENT_POS);
            bool found = false;
            float g = FindGroundZ3D(lp[0], lp[1], lp[2] + 0.5f, &found);
            bool dmgOn = g_dmgPed == locked;
            Log("AimLock: no damage on ped 0x%08X model %d after %d shot%s (locked %.1f s, error %.1f deg): state %d health %.0f "
                "armour %.0f uses collision %d inVehicle %d vehicle 0x%08X pos %.2f %.2f %.2f ground %s %.2f; "
                "trainer InflictDamage on it: %s (%d calls, %d refused)",
                (unsigned)(DWORD)locked, at<short>(locked, ENT_MODEL), newShots, newShots == 1 ? "" : "s",
                (now - lockSince) / 1000.0f, err0 * 57.29578f, at<int>(locked, PED_STATE), at<float>(locked, PED_HEALTH),
                at<float>(locked, PED_ARMOUR), at<unsigned char>(locked, ENT_FLAGS_51) & 1,
                at<unsigned char>(locked, PED_IN_VEHICLE), (unsigned)(DWORD)at<void*>(locked, PED_MY_VEHICLE),
                lp[0], lp[1], lp[2], found ? "found" : "none", found ? g : 0.0f,
                !dmgOn || g_dmgLast < 0 ? "not called" : g_dmgLast ? "true" : "false",
                dmgOn ? g_dmgCalls : 0, dmgOn ? g_dmgRefused : 0);
        }
    }

    // The player takes the camera back with a real flick away from the target:
    // the turn the mouse asked for this tick (as the game's camera turns it,
    // counts x sensitivity x FOV / 80 - VCModernFix hands the game several
    // frames' counts at once, so raw counts alone tripped on normal tracking)
    // over 8 degrees, and away from the target. Not with silent aim (the
    // camera is theirs anyway) or while hopping (strafe turns are no let-go).
    if (!silent && !g_bhopping) {
        float fov = at<float>(ActiveCam(), CAM_FOV);
        float ix = *InvertMouseX ? -1.0f : 1.0f, iy = *InvertMouseY ? -1.0f : 1.0f;
        float askYaw = -2.5f * mx * ix * *MouseSensH * fov / 80.0f;
        float askPitch = 4.0f * my * iy * *MouseSensV * fov / 80.0f;
        float ask = sqrtf(askYaw * askYaw + askPitch * askPitch);
        if (ask > 0.1396f && askYaw * dh0 + askPitch * dp0 < 0.0f) {      // 8 degrees, away from it
            g_as.releases++;
            locked = nullptr; releasedUntil = now + 400; ClearTurn(g_lockTurn); return;
        }
    }

    // shots fired since the last tick, at this target (camera lock only)
    if (!silent && newShots > 0 && !IsHeliTarget(locked) && hpOf == locked)
        for (int k = 0; k < newShots; k++) RecordShot(locked, hpBefore, err0);

    // Shot at while seated in / boarding a car, or down on the ground: the
    // game's bullet can't be relied on - InflictDamage refuses a passenger or a
    // ped getting in / out without collision (states 57 / 60 in the log, health
    // stuck at 1), and the bullet passes over a body on the ground. So a shot on
    // target (within 4 degrees) kills a seated one outright (its car is blown
    // up if it is still alive 3 ticks later), and gives a downed one the gun's
    // damage, or a kill if that shows no drop either. Collision is never
    // changed for a seated ped, and put back for one on foot (SafeDamage).
    if (newShots > 0 && !IsHeliTarget(locked) && IsLivePed(locked) && !PedGone(locked) && err0 < 0.0698f) {
        if (InOrBoardingVehicle(locked)) {
            if (g_seatedKill != locked) StartSeatedKill(locked, me, "aim lock shot on target");
        } else if (PedDown(locked) && hpOf == locked && !ShotLanded(locked, hpBefore)) {
            char* wi = curType >= 0 ? GetWeaponInfo(curType) : nullptr;
            DamageOrKill(locked, me, curType >= 0 ? curType : 45, wi ? fmaxf((float)at<int>(wi, WI_DAMAGE), 25.0f) : 100.0f);
        }
    }

    // The lock lets go of a target it can't hurt: 5 shots on it (within 1
    // degree) with no health drop, or more than 1 s under the map (more than
    // 1 m below the top ground at its x,y with no ground under it). Not picked
    // again for 5 s.
    if (!IsHeliTarget(locked) && IsLivePed(locked)) {
        static void* watchOf = nullptr;
        static int noDamageShots = 0;
        static DWORD underSince = 0;
        if (watchOf != locked) { watchOf = locked; noDamageShots = 0; underSince = 0; }
        if (newShots > 0 && hpOf == locked && err0 < 0.01745f) {
            if (ShotLanded(locked, hpBefore)) noDamageShots = 0;
            else noDamageShots += newShots;
        }
        const float* lp = &at<float>(locked, ENT_POS);
        bool below = false;
        float g = FindGroundZ3D(lp[0], lp[1], lp[2] + 0.5f, &below);
        float top = FindGroundZ(lp[0], lp[1]);
        bool under = !below && lp[2] < top - 1.0f;
        if (!under) underSince = 0;
        else if (!underSince) underSince = now;
        const char* why = noDamageShots >= 5 ? "no damage" : (under && now - underSince > 1000) ? "under the map" : nullptr;
        if (why) {
            static int dropLogs = 0;
            if (dropLogs++ < 200) {
                if (noDamageShots >= 5)
                    Log("AimLock: dropped model %d: no damage after %d shots (state %d health %.0f inVehicle %d collision %d)",
                        at<short>(locked, ENT_MODEL), noDamageShots, at<int>(locked, PED_STATE), at<float>(locked, PED_HEALTH),
                        at<unsigned char>(locked, PED_IN_VEHICLE), at<unsigned char>(locked, ENT_FLAGS_51) & 1);
                else
                    Log("AimLock: dropped model %d: under the map (z %.2f, top ground %.2f, nothing below; state %d health %.0f)",
                        at<short>(locked, ENT_MODEL), lp[2], top, at<int>(locked, PED_STATE), at<float>(locked, PED_HEALTH));
            }
            if (noDamageShots >= 5) g_as.dropNoDamage++; else g_as.dropUnderMap++;
            (void)g;
            block(locked);
            watchOf = nullptr;
            locked = nullptr;
            return;                                                        // the next tick picks another
        }
    }

    // Moving targets. The game's bullet follows the camera as processed in the
    // update after this one (the turn written now is processed then, from
    // where the player is by then), and flies during the update after that -
    // so aim where the target will be 2 ticks on, seen from where the player
    // will be 1 tick on: ap + 2 ticks of its speed - 1 tick of the player's,
    // sideways only (bumps and gravity would bob the aim). The gun itself is
    // put on the target at the instant it fires (FireTimeAim); this keeps the
    // view on the target so that correction stays small.
    {
        float tick = *TimeStep;                                            // game steps (1/50 s) per tick
        if (!(tick > 0.1f && tick < 5.0f)) tick = 50.0f / 30.0f;
        // a ped sitting in a car is carried by it: the car's speed, not its own
        void* carrier = locked;
        if (!IsHeliTarget(locked) && at<unsigned char>(locked, PED_IN_VEHICLE)) {
            void* v = at<void*>(locked, PED_MY_VEHICLE);
            if (v && IsLiveVehicle(v)) carrier = v;
        }
        const float* mv = &at<float>(me, PHYS_MOVE);
        // The scoped sniper's bullet travels 16 units a step: the target moves
        // on while it flies - its speed (not the player's: the bullet doesn't
        // take the shooter's) times the flight time, every tick, never switched
        // on and off (the old heli lead toggled across 0.5 degrees and the
        // zoomed view swung between the heli and a point 1-2 m beside it).
        float dist = sqrtf((ap[0] - src[0]) * (ap[0] - src[0]) + (ap[1] - src[1]) * (ap[1] - src[1]) + (ap[2] - src[2]) * (ap[2] - src[2]));
        float flight = SniperBulletShot(me) ? dist / SNIPER_BULLET_SPEED : 0.0f;   // steps of 1/50 s
        if (IsHeliTarget(locked)) {
            // A police heli's speed wobbles every tick as it hovers: averaged
            // over ~0.25 s. Its instant-hit shots are put on it as they fire
            // (FireTimeAim), so they get no lead; the sniper's travelling bullet
            // gets the flight lead.
            HeliSpeedTick(locked, tick);
            for (int i = 0; i < 3; i++) ap[i] += g_heliV[i] * flight;
            if (flight > 0.0f) g_as.leadOn++; else g_as.leadOff++;
        } else {
            const float* tv = &at<float>(carrier, PHYS_MOVE);
            for (int i = 0; i < 2; i++) ap[i] += (2.0f * tv[i] - mv[i]) * tick + tv[i] * flight;
            g_as.leadOn++;
        }
    }

    // turn the camera so the crosshair lands on the target
    float dh, dp, errLed;
    turnTo(ap, dh, dp, errLed);

    // The camera builds its direction from (cos Beta, sin Beta) and sin(Alpha)
    // (CCam 0x479854..0x4798FE) in third-person and first-person weapon views
    // alike, so the view turns the way Beta grows and looks up as Alpha grows.
    // While bunny hopping the camera is never turned (it would turn the run):
    // the target is still picked, and ShotTick lands each shot on it instead.
    if (silent) return;
    char* cam = ActiveCam();
    bool followCam = at<short>(cam, CAM_MODE) == CAM_MODE_FOLLOWPED;
    // Nearly straight overhead the round angle (from the horizontal part of
    // the direction alone) is undefined and flips by up to 180 degrees as the
    // target drifts: above 60 degrees up, the yaw stays where it is unless the
    // target is over 2 m off to the side.
    float to[3] = { ap[0] - src[0], ap[1] - src[1], ap[2] - src[2] };
    float horiz = sqrtf(to[0] * to[0] + to[1] * to[1]);
    float elev = atan2f(to[2], horiz);
    if (elev > 1.0472f && horiz < 2.0f) dh = 0.0f;
    // Above the mouse camera's 45-degree limit the target can't be brought
    // under the crosshair: aim round only and hold the tilt at the limit.
    bool outOfReach = followCam && elev > CAM_PITCH_MAX;
    // Snap: the whole remaining angle in one tick, on a new target and while
    // following (the turn goes in before the camera is built, so it lands at
    // once). No per-tick cap: the overhead hold and the pitch clamp above are
    // what stopped the shaking.
    const float gain = 1.0f;
    float alpha0 = at<float>(cam, CAM_ALPHA);
    float dBeta = dh * gain, dAlpha = outOfReach ? CAM_PITCH_MAX - alpha0 : dp * gain;
    float alpha1 = alpha0 + dAlpha;
    if (followCam) alpha1 = fminf(CAM_PITCH_MAX, fmaxf(CAM_PITCH_MIN, alpha1));
    QueueTurn(g_lockTurn, cam, dBeta, alpha1 - alpha0);                    // goes in before the next camera update
    if (IsHeliTarget(locked)) {
        heliTicks++;
        if (outOfReach) heliOutOfReach++;
        float m = sqrtf(dBeta * dBeta + (alpha1 - alpha0) * (alpha1 - alpha0));
        if (m > heliMaxTurn) heliMaxTurn = m;
    }
    // what the crosshair should show next tick
    cmdYaw = atan2f(c[1], c[0]) + dBeta;
    cmdPitch = asinf(fmaxf(-1.0f, fminf(1.0f, c[2]))) + (alpha1 - alpha0);
    cmdValid = true;
}

// ---- wall bang / one-shot vehicles --------------------------------------------
// Every shot the player fires (any gun) is counted by the CWeapon::Fire hook.
// Then the shot's line (camera through the crosshair) is followed by the trainer:
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

// (g_playerShots, counted by the CWeapon::Fire hook below, is declared above
// with FireAnyView.)
constexpr float PEN_RANGE = 500.0f;                                        // penetrating bullets reach this far

// While a player's instant-hit shot is fired with "bullet penetration" on, the
// weapon's range (CWeaponInfo +4, read by FireInstantHit) is stretched so the
// game's own bullet - tracer, impact, blood - reaches PEN_RANGE, then put back
// (enemies with the same gun keep their normal range).
struct RangeBoost {
    char* wi = nullptr; float saved = 0.0f;
    RangeBoost(void* w) {
        if (!Active().bulletPenetrate) return;
        char* info = GetWeaponInfo(at<int>(w, WEAP_TYPE));
        if (!info || at<int>(info, WI_FIRETYPE) != 1 || at<float>(info, WI_RANGE) >= PEN_RANGE) return;
        wi = info; saved = at<float>(wi, WI_RANGE); at<float>(wi, WI_RANGE) = PEN_RANGE;
    }
    ~RangeBoost() { if (wi) at<float>(wi, WI_RANGE) = saved; }
};

// ---- fire-time aim ----------------------------------------------------------------
// The player's instant-hit bullet runs along the mouse crosshair of the camera
// as last processed (CCam Front / Source / Up, game.h CAM_FRONT): the camera
// update runs once a tick, after the peds have fired, so a bullet follows the
// view from before the aim lock's latest turn, and the target and the player
// have both moved since. The lock's own measure said 0 degrees while shots
// went past a walking ped (VCTrainer.log 2026-11-19: 599 shots, 82 hit).
// So at the moment the game fires - inside CWeapon::Fire - the processed
// Front and Up are turned, for that one call, by exactly the angle between
// the crosshair line and the locked ped's bone right now (its bone matrices
// rebuilt first: the ped's hit spheres are built from the same matrices), and
// put back after. Only with the camera lock on (not silent aim), on foot, a
// live ped locked, an instant-hit gun, outside the first-person views, and
// within 10 degrees - the lock holds the view on the target; this only takes
// out the latency.
static void RotateAbout(float* v, const float* k, float c, float s) {      // Rodrigues, k unit
    float kv = k[0] * v[0] + k[1] * v[1] + k[2] * v[2];
    float x[3] = { k[1] * v[2] - k[2] * v[1], k[2] * v[0] - k[0] * v[2], k[0] * v[1] - k[1] * v[0] };
    for (int i = 0; i < 3; i++) v[i] = v[i] * c + x[i] * s + k[i] * kv * (1.0f - c);
}
// ---- the crosshair ray at the drawn FOV ---------------------------------------
// The game's Find3rdPersonCamTargetVector builds the shot's ray from the
// crosshair's screen fraction with the CCam FOV (and a 1.8 factor), but the
// frame is drawn with CDraw's FOV, which VC Modern Fix stores Hor+ and the FOV
// slider widens: at a wide FOV the shot leaves the drawn crosshair. Here the
// ray is built from the drawn FOV and aspect, so it passes through the
// crosshair as seen. One function for the hook (hooks.cpp) and the fire-time
// aim below.
static bool CrosshairDir(const char* cam, float mx, float my, float* dir) {
    const float* F = (const float*)(cam + CAM_FRONT);
    const float* U = (const float*)(cam + CAM_UP);
    float aspect = *DrawAspectRatio > 0.1f ? *DrawAspectRatio : 4.0f / 3.0f;
    float tx = tanf(*DrawFov * 0.5f * 3.14159265f / 180.0f), ty = tx / aspect;
    float R[3] = { F[1] * U[2] - F[2] * U[1], F[2] * U[0] - F[0] * U[2], F[0] * U[1] - F[1] * U[0] };   // Cross(Front, Up)
    float a = (1.0f - 2.0f * my) * ty, b = (2.0f * mx - 1.0f) * tx;
    for (int i = 0; i < 3; i++) dir[i] = F[i] + U[i] * a + R[i] * b;
    float l = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (!(l > 0.001f)) return false;
    for (int i = 0; i < 3; i++) dir[i] /= l;
    return true;
}

// The hook's choice: the game's own ray with the trainer off, the FOV slider at
// 70, a first-person camera, or looking behind; otherwise the drawn-FOV ray.
bool WideCrosshairTarget(float dist, const float* pos, float* source, float* target) {
    if (!S.enabled || fabsf(S.fov - 70.0f) < 0.05f) return false;
    const char* cam = ActiveCam();
    if (IsFirstPersonCamMode(*(const short*)(cam + CAM_MODE))) return false;
    void* pad = GetPad(0);
    if (pad && GetLookBehindForPed(pad)) return false;
    float dir[3];
    if (!CrosshairDir(cam, *CrosshairScreenX, *CrosshairScreenY, dir)) return false;
    const float* cs = (const float*)(cam + CAM_SOURCE);
    float d = (pos[0] - cs[0]) * dir[0] + (pos[1] - cs[1]) * dir[1] + (pos[2] - cs[2]) * dir[2];
    for (int i = 0; i < 3; i++) source[i] = cs[i] + d * dir[i];
    for (int i = 0; i < 3; i++) target[i] = source[i] + dist * dir[i];
    return true;
}

static bool CrosshairLine(void* me, float* src, float* dir) {
    const float* mp = &at<float>(me, ENT_POS);
    float tgt[3];
    if (!WideCrosshairTarget(100.0f, mp, src, tgt))                       // the drawn-FOV ray, or the game's own
        Find3rdPersonCamTargetVector(TheCamera, 100.0f, mp[0], mp[1], mp[2], src, tgt);
    for (int i = 0; i < 3; i++) dir[i] = tgt[i] - src[i];
    float l = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (l < 0.001f) return false;
    for (int i = 0; i < 3; i++) dir[i] /= l;
    return true;
}
struct FireTimeAim {
    char* cam = nullptr;
    float front[3], up[3];
    // Also for a locked police heli (the middle of its box), and in the scope:
    // the scoped sniper fires from the active CCam's Source along its Front
    // (CWeapon::FireSniper, game.h) - a travelling bullet, so the point is led
    // by the target's speed times the flight time - and Front is what is
    // turned. Other views take the mouse crosshair line, as before.
    FireTimeAim(void* w, void* me) {
        const Settings& A = Active();
        void* t = g_aimTarget;
        bool silent = A.aimSilent || (g_bhopping && A.aimSilentBhop);
        if (!A.aimLock || silent || !t || PlayerVehicle()) return;
        bool heli = IsHeliTarget(t);
        if (!heli && !IsLivePed(t)) return;
        int type = at<int>(w, WEAP_TYPE);
        char* wi = GetWeaponInfo(type);
        bool sniperRifle = type == WEAPON_SNIPER || type == WEAPON_LASER_SNIPER;
        bool sniper = sniperRifle && FirstPersonWeaponCam();               // FireSniper's travelling bullet
        if (!wi || (at<int>(wi, WI_FIRETYPE) != 1 && !sniper)) return;
        if (!heli) {
            int st = at<int>(t, PED_STATE);
            if (st == PED_DIE || st == PED_DEAD) return;
        }
        char* cc = ActiveCam();
        float cs[3], D[3];
        if (sniper) {
            memcpy(cs, cc + CAM_SOURCE, 12);
            memcpy(D, cc + CAM_FRONT, 12);
            float l = sqrtf(D[0] * D[0] + D[1] * D[1] + D[2] * D[2]);
            if (l < 0.001f) return;
            for (float& v : D) v /= l;
        } else {
            if (!CrosshairLine(me, cs, D)) return;
            // The ray always passes the CCam Source (its start is only the
            // player's spot on it), and turning Front / Up turns it round
            // there: so the target's direction is taken from there. From the
            // player's spot, a target a metre or two away was missed by the
            // gap between the two.
            if (!IsFirstPersonCamMode(at<short>(cc, CAM_MODE))) memcpy(cs, cc + CAM_SOURCE, 12);
        }
        float a[3];
        if (heli) HeliAimPoint(t, a);
        else {
            EntityUpdateRpHAnim(t);                                        // its bones (and hit spheres) as of now
            EnemyAimPoint(t, a, sniperRifle);
        }
        if (sniper) {                                                      // where it will be when the bullet gets there
            float dist = sqrtf((a[0] - cs[0]) * (a[0] - cs[0]) + (a[1] - cs[1]) * (a[1] - cs[1]) + (a[2] - cs[2]) * (a[2] - cs[2]));
            float flight = dist / SNIPER_BULLET_SPEED;
            const float* v = &at<float>(t, PHYS_MOVE);
            if (heli && g_heliVOf == t) v = g_heliV;
            else if (!heli && at<unsigned char>(t, PED_IN_VEHICLE)) {
                void* car = at<void*>(t, PED_MY_VEHICLE);
                if (car && IsLiveVehicle(car)) v = &at<float>(car, PHYS_MOVE);
            }
            for (int i = 0; i < (heli ? 3 : 2); i++) a[i] += v[i] * flight;
        }
        float T[3] = { a[0] - cs[0], a[1] - cs[1], a[2] - cs[2] };
        float tl = sqrtf(T[0] * T[0] + T[1] * T[1] + T[2] * T[2]);
        if (tl < 0.5f) return;
        for (float& v : T) v /= tl;
        float c = fmaxf(-1.0f, fminf(1.0f, D[0] * T[0] + D[1] * T[1] + D[2] * T[2]));
        float ang = acosf(c);
        g_as.fireN++;
        g_as.fireErrSum += ang;
        if (ang > g_as.fireErrMax) g_as.fireErrMax = ang;
        if (ang < 0.0003f) { g_as.fireCorrected++; return; }               // already on it
        if (ang > 0.1745f) { g_as.fireTooFar++; return; }                  // over 10 degrees: not the lock's shot
        float k[3] = { D[1] * T[2] - D[2] * T[1], D[2] * T[0] - D[0] * T[2], D[0] * T[1] - D[1] * T[0] };
        float s = sqrtf(k[0] * k[0] + k[1] * k[1] + k[2] * k[2]);
        if (s < 1e-6f) return;
        for (float& v : k) v /= s;
        memcpy(front, cc + CAM_FRONT, 12);
        memcpy(up, cc + CAM_UP, 12);
        cam = cc;
        // Front keeps its length (FireSniper normalises it anyway)
        RotateAbout((float*)(cc + CAM_FRONT), k, c, s);
        RotateAbout((float*)(cc + CAM_UP), k, c, s);
        // did the shot's line follow? (the joystick crosshair doesn't use Front)
        float cs2[3], D2[3];
        bool followed;
        if (sniper) {
            memcpy(D2, cc + CAM_FRONT, 12);
            float l = sqrtf(D2[0] * D2[0] + D2[1] * D2[1] + D2[2] * D2[2]);
            followed = l > 0.001f && (D2[0] * T[0] + D2[1] * T[1] + D2[2] * T[2]) / l > cosf(ang * 0.5f);
            if (followed) g_as.fireSniper++;
        } else followed = CrosshairLine(me, cs2, D2) && D2[0] * T[0] + D2[1] * T[1] + D2[2] * T[2] > cosf(ang * 0.5f);
        if (followed) g_as.fireCorrected++;
        else g_as.fireNotApplied++;
    }
    ~FireTimeAim() {
        if (!cam) return;
        memcpy(cam + CAM_FRONT, front, 12);
        memcpy(cam + CAM_UP, up, 12);
    }
};

bool TrainerWeaponFire(void* w, void* shooter, float* source, bool (__thiscall* orig)(void*, void*, float*)) {
    if (!w || !shooter || shooter != FindPlayerPed()) return orig(w, shooter, source);
    int clip = at<int>(w, WEAP_CLIP), total = at<int>(w, WEAP_TOTAL), timer = at<int>(w, WEAP_TIMER);
    bool ok;
    {
        FireTimeAim aim(w, shooter);
        RangeBoost boost(w);
        // Damage multiplier: the gun's damage (CWeaponInfo, shared with every
        // ped holding it) is raised for just this call - the instant-hit
        // bullet applies it inside - then put back.
        char* info = nullptr; int savedDmg = 0;
        float mult = Active().damageMult;
        if (fabsf(mult - 1.0f) > 0.01f && (info = GetWeaponInfo(at<int>(w, WEAP_TYPE)))) {
            savedDmg = at<int>(info, WI_DAMAGE);
            at<int>(info, WI_DAMAGE) = (int)fmaxf(1.0f, savedDmg * mult);
        }
        ok = orig(w, shooter, source);
        if (info) at<int>(info, WI_DAMAGE) = savedDmg;
    }
    if (ok && (at<int>(w, WEAP_CLIP) != clip || at<int>(w, WEAP_TOTAL) != total || at<int>(w, WEAP_TIMER) != timer)) {
        g_playerShots = g_playerShots + 1;
        if (!g_trainerFiring) g_gameShotAt = GetTickCount();               // the game's own shot (not rapid fire's)
    }
    return ok;
}

static bool ThroughWalls(const Settings& A) { return A.wallBang || A.bulletPenetrate; }

void BlowUpVehicle(void* v, void* me) {
    if (at<int>(v, VEH_TYPE) == 3) { ShootDownHeli(v); return; }          // police helicopter
    at<unsigned char>(v, VEH_FLAGS_1FB) |= 0x40;                           // can be damaged
    at<unsigned char>(v, PHYS_FLAGS1) &= ~0x02;                            // not explosion proof
    at<unsigned char>(v, PHYS_FLAGS2) &= ~0x06;                            // not bullet / fire proof
    at<float>(v, VEH_HEALTH) = 0.0f;
    VehicleBlowUp(v, me);
}

void ShotTick(const Settings& A, void* me) {
    static int seen = 0;
    int shots = g_playerShots;
    bool fired = shots != seen;
    seen = shots;
    // A Hunter / Sea Sparrow shot (rocket pod or cannon): its line starts at the
    // gun (the source the game passed) and runs along the heli's nose.
    static int seenHeli = 0;
    int heliCount = g_heliShots;
    void* heli = PlayerVehicle();
    bool heliShot = heliCount != seenHeli && IsHunterModel(heli) && g_heliShotType >= 0;
    seenHeli = heliCount;
    bool silent = !heliShot && (A.aimSilent || (g_bhopping && A.aimSilentBhop)) && A.aimLock && g_aimTarget;   // silent aim (always, or while hopping)
    if (!fired || (!A.wallBang && !A.oneShotVehicles && !A.bulletPenetrate && !silent) || *GameMenuActive) return;
    int type;
    if (heliShot) type = g_heliShotType;
    else {
        int slot = at<signed char>(me, PED_CUR_WEAPON);
        if (slot < 0 || slot >= WEAPON_SLOTS) return;
        type = at<int>((char*)me + PED_WEAPONS + slot * WEAPON_SIZE, WEAP_TYPE);
    }
    char* wi = GetWeaponInfo(type);
    if (!wi) return;
    int fire = at<int>(wi, WI_FIRETYPE);                                   // 1 instant hit, 2 projectile
    if (fire != 1 && fire != 2) return;
    bool pen = A.bulletPenetrate && fire == 1;
    float range = fire == 1 ? (pen ? PEN_RANGE : fminf(fmaxf(at<float>(wi, WI_RANGE), 30.0f), 300.0f)) : 200.0f;
    float damage = fmaxf((float)at<int>(wi, WI_DAMAGE), 25.0f);
    float src[3], dir[3];
    if (heliShot) {
        const float* f = &at<float>(heli, ENT_FWD);
        float l = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
        if (l < 0.001f) return;
        for (int i = 0; i < 3; i++) { src[i] = g_heliShotSrc[i]; dir[i] = f[i] / l; }
    } else if (!CrosshairRay(me, src, dir)) return;
    void* myVeh = PlayerVehicle();
    bool through = ThroughWalls(A);

    // Silent aim (bunny hopping with the aim lock on): the shot lands on the
    // locked target wherever the camera points - in sight (fences and glass
    // don't count), or anywhere with wall bang / penetration.
    if (silent) {
        void* t = g_aimTarget;
        if (IsHeliTarget(t)) {
            if (A.oneShotVehicles || fire == 2) ShootDownHeli(t);
        } else if (IsLivePed(t) && at<float>(t, PED_HEALTH) > 0.0f) {
            if (through || PedReachable(me, src, t))
                DamageOrKill(t, me, type, fire == 2 ? 1000.0f : damage);
        }
        if (!A.wallBang && !A.oneShotVehicles && !pen) return;
    }

    // Everything on the shot's line - vehicles and people - nearest first.
    struct Hit { void* e; float t; bool veh; };
    Hit hits[48]; int n = 0;
    auto add = [&](void* e, float t, bool veh) {
        if (n == 48) return;
        int i = n++;
        while (i > 0 && hits[i - 1].t > t) { hits[i] = hits[i - 1]; i--; }
        hits[i] = { e, t, veh };
    };
    if (A.oneShotVehicles || pen) {
        char* pool = (char*)*VehiclePool;
        float vrange = A.oneShotVehicles ? fmaxf(range, 400.0f) : range;   // helicopters fly high and far
        if (pool) {
            char* objs = *(char**)(pool + POOL_OBJECTS);
            unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
            int size = *(int*)(pool + POOL_SIZE);
            for (int i = 0; i < size; i++) {
                if (flags[i] & 0x80) continue;
                void* v = objs + i * VEHICLE_POOL_STRIDE;
                if (v == myVeh) continue;
                if (at<int>(v, VEH_TYPE) == 3 ? !IsHeliTarget(v) : at<float>(v, VEH_HEALTH) <= 0.0f) continue;
                float t;
                if (RayHitsVehicle(src, dir, vrange, v, &t)) add(v, t, true);
            }
        }
    }
    if ((through && fire == 1) || pen) {
        char* pool = (char*)*PedPool;
        if (pool) {
            char* objs = *(char**)(pool + POOL_OBJECTS);
            unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
            int size = *(int*)(pool + POOL_SIZE);
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
                if (t < 1.0f || t > range) continue;
                float px = d[0] - dir[0] * t, py = d[1] - dir[1] * t, pz = d[2] - dir[2] * t;
                if (sqrtf(px * px + py * py + pz * pz) > 0.55f + t * 0.004f) continue;   // on the line (wider far away)
                add(p, t, false);
            }
        }
    }

    // The game's own bullet lands on the first thing on the line if no wall
    // is in front of it; everything else the trainer hits:
    //  - one-shot vehicles: the first vehicle in reach blows up (with
    //    penetration, every vehicle on the line)
    //  - wall bang: the first person on the line behind a wall or a car
    //  - penetration: every person and vehicle on the line, walls or not
    bool gameBullet = true, vehDone = false, pedDone = false;
    for (int i = 0; i < n; i++) {
        const Hit& h = hits[i];
        float pt[3];
        if (h.veh) for (int k = 0; k < 3; k++) pt[k] = src[k] + dir[k] * h.t;
        else EnemyAimPoint(h.e, pt);
        bool open = LineOfSightClear(src, pt, true, !h.veh, false, true, false, true, true);   // no wall (or car, for people) in front; glass/fences don't count
        bool byGame = gameBullet && open && h.t <= range;
        gameBullet = false;                                                // the game's bullet stops at the first thing
        if (h.veh) {
            if (A.oneShotVehicles && !vehDone && (open || through)) { BlowUpVehicle(h.e, me); vehDone = !pen; }
            else if (pen && !byGame && at<int>(h.e, VEH_TYPE) != 3) {
                at<unsigned char>(h.e, VEH_FLAGS_1FB) |= 0x40;            // can be damaged
                at<float>(h.e, VEH_HEALTH) -= damage;                      // below 250 it burns and blows up
            }
        } else {
            if (!pedDone && !byGame && through)
                DamageOrKill(h.e, me, type, damage);
            if (!pen) pedDone = true;                                      // wall bang: just the first person
        }
    }
}

// ---- genocide ------------------------------------------------------------------
// While on, every ped around dies and every vehicle around blows up - the
// player, the player's vehicle and anyone riding in it excepted. New ones
// that stream in go the same way. Vehicles go a few at a time so the
// explosions don't stall the game.
// ---- drive-by aimbot ------------------------------------------------------------
// The aim lock for shooting from a vehicle (drive-by, passenger seat, bike).
// While the right mouse button is held (otherwise drive-bys are the game's
// own normal shooting) the nearest enemy all round is locked, the same enemies as
// the aim lock (gangs, police when wanted, criminals, attackers; never anyone
// riding with you), in sight unless wall bang / penetration is on. It follows
// the aim lock's settings:
//  - Silent aim off: the camera is turned onto the enemy like the on-foot
//    aim lock, and Snap camera back returns it when you stop.
//  - Silent aim on: the camera is never turned.
// Either way every drive-by bullet you fire (counted by the CWeapon::Fire
// hook) lands on the locked enemy with the gun's damage: a driver's bullets
// leave the side of the car, which the camera alone doesn't steer.
static void* g_driveByTarget = nullptr;
void DriveByAimTick(const Settings& A, void* me) {
    static int seen = 0;
    static void* locked = nullptr;
    static DWORD shotAt = 0;
    // (g_driveByTurn: queued, put in before the next camera update)
    g_driveByTurn.resetOnCamChange = true;                                 // look left / right is another camera
    ApplyTurn(g_driveByTurn);                                              // the pad hook didn't run: now
    DWORD now = GetTickCount();
    int shots = g_playerShots;
    int fired = shots - seen;
    seen = shots;
    // the Hunter's own rockets / cannon are counted there too: not drive-by bullets
    static int seenHeli = 0, seenExtra = 0;
    int heli = g_heliShots, extra = g_driveByExtraShots;
    fired -= heli - seenHeli;
    if (fired < 0) fired = 0;
    seenHeli = heli;
    // rapid fire's added drive-by shots (RapidFireTick): each lands like the game's
    int added = extra - seenExtra;
    seenExtra = extra;
    if (added > 0) fired += added;                                        // (the ammo count below sees them too: the larger wins)
    // Drive-by shots don't go through CWeapon::Fire (its only callers are the
    // on-foot ped code), so the hook never counts them: read them off the gun
    // in hand instead - ammo used, or its fire timer restarted (a new shot;
    // also covers infinite ammo).
    {
        static int lastType = -1, lastTotal = 0, lastClip = 0, lastTimer = 0;
        int slot = at<signed char>(me, PED_CUR_WEAPON);
        if (PlayerVehicle() && slot >= 0 && slot < WEAPON_SLOTS) {
            char* w = (char*)me + PED_WEAPONS + slot * WEAPON_SIZE;
            int t = at<int>(w, WEAP_TYPE), total = at<int>(w, WEAP_TOTAL), clip = at<int>(w, WEAP_CLIP), timer = at<int>(w, WEAP_TIMER);
            if (t == lastType) {
                int used = lastTotal - total;
                if (used <= 0) used = lastClip - clip;                     // total not lowered: count the clip
                if (used < 0 || used > 10) used = 0;                       // reload / pickup / cheat refill
                if (used == 0 && timer > lastTimer && clip <= lastClip && total <= lastTotal) used = 1;   // not a reload
                if (used > fired) fired = used;
            }
            lastType = t; lastTotal = total; lastClip = clip; lastTimer = timer;
        } else lastType = -1;
    }
    if (fired > 0) shotAt = now;
    g_driveByTarget = nullptr;
    auto turnBack = [] { QueueTurnBack(g_driveByTurn); };
    void* veh = PlayerVehicle();
    // only while the right mouse button is held: otherwise drive-bys are the
    // game's normal shooting
    bool aiming = veh && !g_menuOpen && GameFocused() && NewMouseState[1] != 0;
    int type = -1; char* wi = nullptr;
    if (veh) {
        int slot = at<signed char>(me, PED_CUR_WEAPON);
        if (slot >= 0 && slot < WEAPON_SLOTS) type = at<int>((char*)me + PED_WEAPONS + slot * WEAPON_SIZE, WEAP_TYPE);
        wi = type >= 0 ? GetWeaponInfo(type) : nullptr;
    }
    bool gun = wi && at<int>(wi, WI_FIRETYPE) == 1;
    if (!A.aimDriveBy || !veh || !gun || !aiming || *GameMenuActive) {
        if (A.aimDriveBy && A.aimSnapBack && !A.aimSilent && veh && !aiming) turnBack();
        else ClearTurn(g_driveByTurn);
        locked = nullptr; return;
    }
    bool silent = A.aimSilent;
    bool pen = A.bulletPenetrate;
    float range = pen ? PEN_RANGE : fminf(fmaxf(at<float>(wi, WI_RANGE), 40.0f), 120.0f);
    float damage = fmaxf((float)at<int>(wi, WI_DAMAGE), 25.0f);
    bool through = ThroughWalls(A);

    const float* src = CameraPos;
    const float* c = CameraFront;
    const float* vp = &at<float>(veh, ENT_POS);
    auto inSight = [&](const float* a) {
        if (through) return true;
        float eye[3] = { vp[0], vp[1], vp[2] + 0.8f };
        return LineOfSightClear(src, a, true, false, false, false, false, true, true) ||
               LineOfSightClear(eye, a, true, false, false, false, false, true, true);
    };
    auto measure = [&](const float* a, float& dist, float& ang) {
        float d[3] = { a[0] - src[0], a[1] - src[1], a[2] - src[2] };
        dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        float dot = dist > 0.01f ? (d[0] * c[0] + d[1] * c[1] + d[2] * c[2]) / dist : 1.0f;
        ang = acosf(fmaxf(-1.0f, fminf(1.0f, dot)));
    };

    // A ped counts as sitting in a vehicle only while that vehicle is still a
    // live, unwrecked one: a ped killed with its car (one-shot vehicles, heavy
    // car, genocide) can keep its seat flag until the game removes it, and its
    // position is the wreck's.
    auto seatedIn = [](void* p) -> void* {
        if (!at<unsigned char>(p, PED_IN_VEHICLE)) return nullptr;
        return at<void*>(p, PED_MY_VEHICLE);
    };
    auto carGone = [](void* v) { return !v || !IsLiveVehicle(v) || VehicleWrecked(v); };
    auto slotOf = [](void* p) -> int {
        char* pool = (char*)*PedPool;
        if (!pool || !p) return -1;
        return (int)(((char*)p - *(char**)(pool + POOL_OBJECTS)) / PED_POOL_STRIDE);
    };

    // Confirm what the lock sees (VCTrainer.log): the locked ped's state, seat
    // flag, vehicle and position, once per tick while locked (600 lines a game
    // session at most - about 20 s of drive-by).
    static int stateLogs = 0;
    if (locked && IsLivePed(locked) && stateLogs < 600) {
        stateLogs++;
        const float* lp = &at<float>(locked, ENT_POS);
        void* lv = at<void*>(locked, PED_MY_VEHICLE);
        Log("DriveBy: ped#%d state %d inVehicle %d vehicle 0x%08X (%s) health %.0f pos %.1f %.1f %.1f",
            slotOf(locked), at<int>(locked, PED_STATE), at<unsigned char>(locked, PED_IN_VEHICLE), (unsigned)(DWORD)lv,
            !lv ? "none" : carGone(lv) ? "wrecked/gone" : "live", at<float>(locked, PED_HEALTH), lp[0], lp[1], lp[2]);
    }

    // keep the locked enemy while it's still a threat, in its car (if it was in
    // one), in range and in sight; otherwise pick again this same tick
    static void* lockedVeh = nullptr;                                     // the vehicle it sat in when locked
    void* before = locked;
    const char* why = nullptr;
    if (locked) {
        float a[3], dist, ang;
        if (!IsLivePed(locked) || at<int>(locked, PED_STATE) == PED_DIE || at<int>(locked, PED_STATE) == PED_DEAD ||
            at<float>(locked, PED_HEALTH) <= 0.0f) why = "died";
        else if (lockedVeh && (seatedIn(locked) != lockedVeh || carGone(lockedVeh))) why = carGone(lockedVeh) ? "died" : "left car";
        else if (!IsHostile(locked, me)) why = "not hostile";
        else {
            EnemyAimPoint(locked, a); measure(a, dist, ang);
            if (dist > range) why = "out of range";
            else if (!inSight(a)) why = "out of sight";
        }
        if (why) locked = nullptr;
    }
    if (!locked) {
        char* pool = (char*)*PedPool; if (!pool) return;
        char* objs = *(char**)(pool + POOL_OBJECTS);
        unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
        int size = *(int*)(pool + POOL_SIZE);
        float best = 1e9f;
        for (int i = 0; i < size; i++) {
            if (flags[i] & 0x80) continue;
            void* p = objs + i * PED_POOL_STRIDE;
            if (!IsHostile(p, me)) continue;                              // also skips our own riders
            void* pv = seatedIn(p);
            if (at<unsigned char>(p, PED_IN_VEHICLE) && carGone(pv)) continue;   // its car is a wreck
            float a[3], dist, ang; EnemyAimPoint(p, a); measure(a, dist, ang);
            if (dist < 1.0f || dist > range) continue;
            // like the aim lock: silent - nearest the crosshair; camera - nearest, crosshair a little sooner
            float score = silent ? ang * 30.0f + dist * 0.05f : dist * (1.0f + ang * 0.5f);
            // one-shot vehicles: the car (and whoever is in it) goes with the
            // first shot anyway - at an equal score the one on foot comes first
            if (A.oneShotVehicles && pv) score *= 1.05f;
            if (score >= best || !inSight(a)) continue;
            best = score; locked = p;
        }
        lockedVeh = locked ? seatedIn(locked) : nullptr;
    }
    if (locked != before || why) {                                        // (left the car: may be picked again on foot)
        static int transitions = 0;
        if (transitions < 300) {
            transitions++;
            char from[16], to[16];
            if (before) _snprintf(from, sizeof from, "ped#%d", slotOf(before)); else strcpy(from, "none");
            if (locked) _snprintf(to, sizeof to, "ped#%d", slotOf(locked)); else strcpy(to, "none");
            from[15] = to[15] = 0;
            Log("DriveBy: lock %s -> %s (reason: %s)%s", from, to, why ? why : "new target",
                locked ? "" : (A.aimSnapBack && !silent ? ", camera turned back" : ", camera left"));
        }
    }
    if (!locked) {                                                        // nobody left: back to the player's view now
        if (A.aimSnapBack && !silent) turnBack(); else ClearTurn(g_driveByTurn);
        return;
    }
    g_driveByTarget = locked;

    // the shots fired this frame land on it
    for (int k = 0; k < (fired > 5 ? 5 : fired) && IsLivePed(locked); k++) {
        int st = at<int>(locked, PED_STATE);
        if (st == PED_DIE || st == PED_DEAD) break;
        // collision off for an animation (getting into / out of a car) makes
        // InflictDamage refuse the hit
        if (!TrainerDamage(locked, me, type, damage)) {
            if (InOrBoardingVehicle(locked)) {
                // Seated / boarding: never its collision bit (that froze the game
                // at 0x42335C). Killed the way TouchKillTick does.
                static int seatedLogs = 0;
                InflictDamage(locked, me, 45 /* UNIDENTIFIED */, 1000.0f, 0, 0);
                at<float>(locked, PED_ARMOUR) = 0.0f;
                at<float>(locked, PED_HEALTH) = 0.0f;
                if (seatedLogs < 200) {
                    seatedLogs++;
                    Log("DriveBy: seated target 0x%08X model %d state %d inVehicle %d vehicle 0x%08X killed without the collision write",
                        (unsigned)(DWORD)locked, at<short>(locked, ENT_MODEL), at<int>(locked, PED_STATE),
                        at<unsigned char>(locked, PED_IN_VEHICLE), (unsigned)(DWORD)at<void*>(locked, PED_MY_VEHICLE));
                }
                break;
            }
            // Diagnostic: every time the drive-by forces a ped's collision on
            static int forcedLogs = 0;
            bool logIt = forcedLogs < 1000;
            if (logIt) {
                forcedLogs++;
                const float* lp = &at<float>(locked, ENT_POS);
                Log("DriveBy: collision forced on ped 0x%08X model %d (was %d): state %d inVehicle %d vehicle 0x%08X health %.0f pos %.2f %.2f %.2f",
                    (unsigned)(DWORD)locked, at<short>(locked, ENT_MODEL), at<unsigned char>(locked, ENT_FLAGS_51) & 1,
                    at<int>(locked, PED_STATE), at<unsigned char>(locked, PED_IN_VEHICLE),
                    (unsigned)(DWORD)at<void*>(locked, PED_MY_VEHICLE), at<float>(locked, PED_HEALTH), lp[0], lp[1], lp[2]);
            }
            unsigned char was = at<unsigned char>(locked, 0x51);
            at<unsigned char>(locked, 0x51) = was | 1;                    // on foot: for this call only
            bool r = TrainerDamage(locked, me, type, damage);
            at<unsigned char>(locked, 0x51) = (at<unsigned char>(locked, 0x51) & ~1) | (was & 1);
            if (logIt) Log("DriveBy:   InflictDamage after forcing: %s (collision put back to %d)", r ? "true" : "false", was & 1);
        }
    }
    if (silent) return;

    // turn the camera onto it (as the aim lock does), leading a moving target
    float ap[3]; EnemyAimPoint(locked, ap);
    const float* tv = &at<float>(locked, PHYS_MOVE);
    const float* mv = &at<float>(veh, PHYS_MOVE);
    for (int i = 0; i < 2; i++) ap[i] += (tv[i] - mv[i]) * 2.0f;
    float d[3] = { ap[0] - src[0], ap[1] - src[1], ap[2] - src[2] };
    float dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (dl < 0.5f) return;
    for (float& v : d) v /= dl;
    float dh = atan2f(d[1], d[0]) - atan2f(c[1], c[0]);
    while (dh > 3.14159265f) dh -= 6.2831853f;
    while (dh < -3.14159265f) dh += 6.2831853f;
    // Sideways only: the car cameras pull their own tilt (Alpha) back every
    // frame, so tilting fought them and bobbed the view up and down. The
    // height doesn't matter for the hit - the shots are landed on the target.
    // (camera changed - look left / right: ApplyTurn starts the count again)
    char* cam = ActiveCam();
    if (fabsf(dh) < 0.01f) return;                                    // on it: no tiny jitter
    // nearly overhead (a heli above the car): the round angle is undefined - hold it
    float horiz = sqrtf((ap[0] - src[0]) * (ap[0] - src[0]) + (ap[1] - src[1]) * (ap[1] - src[1]));
    if (atan2f(ap[2] - src[2], horiz) > 1.0472f && horiz < 2.0f) return;
    const float gain = 0.6f;                                          // smooth, no overshoot against the car cam
    float dBeta = dh * gain, dAlpha = 0.0f;
    CapTurn(dBeta, dAlpha);                                           // 30 degrees a tick at most
    QueueTurn(g_driveByTurn, cam, dBeta, 0.0f);                       // goes in before the next camera update
}

void GenocideTick(const Settings& A, void* me) {
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
// In a vehicle the game fires drive-bys itself, not through CWeapon::Fire:
// with vehicle fire held and game time past the gun's timer it calls
// CWeapon::FireFromCar (0x5D44E0) and then sets timer = now + 70 ms - a fixed
// ~14 shots a second for every drive-by gun. The three places that do it
// (left / right / bike) each have that 70 as one byte in "add eax, 0x46";
// while rapid fire is on it is 20 ms instead (as fast as the frames allow,
// up to ~50 shots a second), and put back to 70 when it's off.
static void DriveByFireRate(bool rapid) {
    static unsigned char* const imm[3] = { (unsigned char*)0x5C94A8, (unsigned char*)0x5C973E, (unsigned char*)0x5C99EA };
    static int applied = -1;
    int want = rapid ? 1 : 0;
    if (want == applied) return;
    for (auto p : imm) {
        if (p[-2] != 0x83 || p[-1] != 0xC0) return;                    // not the v1.0 code we know: leave it
    }
    for (auto p : imm) {
        DWORD old;
        VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old);
        *p = rapid ? 20 : 0x46;
        VirtualProtect(p, 1, old, &old);
    }
    applied = want;
}

// The game fires drive-bys only in its update (DoDriveByShootings, once per
// 30 Hz tick), so the 20 ms timer above still gives one shot a tick at most.
// On a tick where the game's drive-by went off (seen by the FireFromCar hooks),
// rapid fire adds the on-foot rate's 22 shots a second on top - 22 / 30 of a
// shot per tick, carried over - with CWeapon::FireFromCar (0x5D44E0,
// thiscall(vehicle, left, right)): the same gun, vehicle and side the game
// chose, so they leave the same window, take ammo and reload as the game's.
// They are counted for DriveByAimTick, which lands them on the locked target.
static const auto FireFromCar = (bool(__thiscall*)(void* weapon, void* veh, int left, int right))0x5D44E0;
static void DriveByRapidTick(const Settings& A, void* me) {
    static float carry = 0.0f;
    static unsigned lastTick = 0;
    void* veh = PlayerVehicle();
    if (!A.rapidFire || !veh || !g_dbShot.valid || g_dbShot.veh != veh || *GameMenuActive) { carry = 0.0f; return; }
    unsigned now = *TimeMs;
    if (g_dbShot.at != now || lastTick == now) return;                  // the game didn't fire this tick (or done already)
    lastTick = now;
    int slot = at<signed char>(me, PED_CUR_WEAPON);
    if (slot < 0 || slot >= WEAPON_SLOTS) return;
    char* w = (char*)me + PED_WEAPONS + slot * WEAPON_SIZE;
    if (w != g_dbShot.w) { carry = 0.0f; return; }                       // gun switched since
    carry += 22.0f / 30.0f;
    int n = (int)carry;
    carry -= (float)n;
    // FireFromCar itself ends every shot with timer = now + 1000 ms (0x5D45C3:
    // add eax, 3E8h), or now + the reload time when the clip ran out (0x5D45A0).
    // The game's own call site overwrites that at once with timer = now + the
    // drive-by interval ("add eax, 46h" at 0x5C94A8 / 0x5C973E / 0x5C99EA). An
    // extra shot has no such caller, so each one held the gun for a second - a
    // burst, a second's stall, a burst. After each extra shot the timer is set
    // the way the game's call site sets it, so fire runs on without a break.
    const unsigned gap = *(volatile unsigned char*)0x5C94A8;               // 20 with rapid fire (the game's 70 without)
    for (int k = 0; k < n; k++) {
        if (at<int>(w, WEAP_STATE) == WEAPSTATE_RELOADING || at<int>(w, WEAP_CLIP) <= 0) break;
        if (!FireFromCar(w, veh, g_dbShot.left, g_dbShot.right)) break;
        at<unsigned>(w, WEAP_TIMER) = now + gap;
        g_driveByExtraShots = g_driveByExtraShots + 1;
    }
}

void RapidFireTick(const Settings& A, void* me) {
    static DWORD nextShot = 0;
    DriveByFireRate(A.rapidFire);
    DriveByRapidTick(A, me);
    static DWORD heldSince = 0;
    if (!A.rapidFire || *GameMenuActive || g_menuOpen || !GameFocused() || PlayerVehicle()) { heldSince = 0; return; }
    if (!NewMouseState[0]) { heldSince = 0; return; }                      // fire button held
    // A click is one normal shot; only holding the button (a burst) turns
    // rapid fire on, so a tap never fires twice.
    if (!heldSince) heldSince = GetTickCount();
    if (GetTickCount() - heldSince < 250) return;
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
    // Only when the game itself would be shooting: the ped attacking / aiming
    // (or the game's own gun went off just now - run and gun fires on the
    // move), or the trainer's hop / fly, which hold the body themselves - and
    // turned to where the camera looks (within ~25 degrees). Otherwise the
    // shot left from a body still turning towards the camera, and with the
    // free camera in front the bullet's line ran back through the player.
    // Such a slot is skipped, not delayed: the rate stays the same.
    if (!g_flyingOnFoot) {
        int st = at<int>(me, PED_STATE);
        bool attacking = st == PED_ATTACK || st == PED_AIM_GUN || g_bhopping || now - g_gameShotAt < 200;
        if (!attacking) { g_as.rapidSkipState++; return; }
        const float* f = &at<float>(me, ENT_FWD);
        float fl = sqrtf(f[0] * f[0] + f[1] * f[1]), cl = sqrtf(CameraFront[0] * CameraFront[0] + CameraFront[1] * CameraFront[1]);
        if (fl > 0.01f && cl > 0.01f && (f[0] * CameraFront[0] + f[1] * CameraFront[1]) / (fl * cl) < 0.9063f) {   // cos 25
            g_as.rapidSkipHeading++; return;
        }
    }
    // The game's own shot, from the muzzle - one of these alone does not
    // need the weapon's animation or a fresh click, so holding the button
    // keeps it going.
    if (FireAnyView(w, me)) g_as.rapidFired++;
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
void NoScopeSniperTick(const Settings& A, void* me) {
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
    bool onTarget = !A.aimLock || !g_aimTarget || A.aimSilent || (g_bhopping && A.aimSilentBhop) || g_aimError < 0.02f;
    if (!onTarget && now - pendingSince < 160) return;                     // let the lock swing on first
    pending = false;

    float source[3];
    MuzzlePos(me, w, source);                                              // the gun, not the chest
    void* target = A.aimLock ? g_aimTarget : nullptr;
    bool rocket = type == WEAPON_ROCKET_LAUNCHER;
    if (rocket) {
        if (!FireRocketAtCrosshair(w, me, source, target)) return;          // nothing launched: no ammo used
        if (g_flyingOnFoot) PushProjectilesOut(me);
    }
    else { RangeBoost boost(w); FireInstantHit(w, me, source); }
    g_playerShots = g_playerShots + 1;                                     // one-shot vehicles, wall bang, penetration
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
            float best = A.bulletPenetrate ? PEN_RANGE : 300.0f;
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
        if (A.wallBang || A.bulletPenetrate || LineOfSightClear(src, a, true, true, false, true, true, true, true))
            DamageOrKill(target, me, type, fmaxf((float)at<int>(wi, WI_DAMAGE), 250.0f));
    }
}

// ---- sniper full auto (scoped) ------------------------------------------------
// In the scope the game wants a fresh click per shot; with this on, holding
// the button fires the game's own sniper shot (CWeapon::Fire -> FireSniper,
// sound and all) about 11 times a second.
void SniperAutoTick(const Settings& A, void* me) {
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

// ---- enemy markers -----------------------------------------------------------
// Every hostile ped (same rule as the aim lock) projected to the screen with
// the game's own CSprite::CalcScreenCoors - through walls - plus the ones off
// screen as a direction. Drawn by the menu's overlay.
// Two halves: what changes with the game (who is hostile, distance, line of
// sight, locked) is worked out once per tick (EnemyScanTick, 30 a second) into
// a list indexed by pool slot and the slot's id byte; the screen position is
// worked out every drawn frame from the ped's position right now, so the boxes
// follow the camera VCModernFix blends between ticks.
std::vector<EspEntry> g_espList;
static double g_espScanMs = 0.0;
static int g_espScans = 0, g_espMarks = 0;

void TakeEspScanStats(double* totalMs, int* scans, int* marks) {
    *totalMs = g_espScanMs; *scans = g_espScans; *marks = g_espMarks;
    g_espScanMs = 0.0; g_espScans = g_espMarks = 0;
}

void EnemyScanTick(const Settings& A, void* me) {
    g_espList.clear();
    if (!A.enemyEsp || !me || *GameMenuActive) return;
    char* pool = (char*)*PedPool; if (!pool) return;
    static LARGE_INTEGER freq;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER t0; QueryPerformanceCounter(&t0);
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
        EspEntry e;
        e.slot = (short)i; e.id = flags[i]; e.dist = dist;
        e.locked = p == g_aimTarget || (p && p == g_driveByTarget);
        // the box from the bones: head to feet standing, along the body when down
        float chest[3], head[3];
        EnemyAimPoint(p, chest);
        EnemyAimPoint(p, head, true);
        int st = at<int>(p, PED_STATE);
        bool down = st == PED_FALL || st == PED_GETUP;
        for (int k = 0; k < 3; k++) {
            e.top[k] = head[k] - pp[k];
            e.bottom[k] = down ? chest[k] - pp[k] : 0.0f;
        }
        e.top[2] += down ? 0.3f : 0.2f;
        e.bottom[2] = down ? fminf(chest[2], head[2]) - pp[2] - 0.3f : -1.0f;
        float eye[3] = { cp[0], cp[1], cp[2] };
        e.visible = LineOfSightClear(eye, chest, true, false, false, false, false, true, true);
        g_espList.push_back(e);
    }
    LARGE_INTEGER t1; QueryPerformanceCounter(&t1);
    g_espScanMs += (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)freq.QuadPart;
    g_espScans++;
    g_espMarks += (int)g_espList.size();
}

const std::vector<EnemyMark>& EnemyMarks() {
    static std::vector<EnemyMark> out;                 // kept: no allocation per frame
    out.clear();
    if (!Active().enemyEsp || !PlayerPed() || *GameMenuActive || g_espList.empty()) return out;
    char* pool = (char*)*PedPool; if (!pool) return out;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    const float* cp = CameraPos;
    for (const EspEntry& e : g_espList) {
        if (e.slot >= size || flags[e.slot] != e.id) continue;   // gone since the tick (or the slot reused)
        void* p = objs + e.slot * PED_POOL_STRIDE;
        const float* pp = &at<float>(p, ENT_POS);
        EnemyMark m{};
        m.dist = e.dist; m.locked = e.locked; m.visible = e.visible;
        float head[3] = { pp[0] + e.top[0], pp[1] + e.top[1], pp[2] + e.top[2] };
        float feet[3] = { pp[0] + e.bottom[0], pp[1] + e.bottom[1], pp[2] + e.bottom[2] };
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

// ---- heli aimbot (Hunter vigilante) -----------------------------------------------
// In a Hunter or Sea Sparrow, holding fire takes out the targets for you, through
// walls, from any direction and height: every 0.25 s one within 400 m across the
// ground (any height above or below) is taken by the player - a car is blown up
// with everyone in it, someone on foot is killed - so missions count each kill.
//  - the vigilante mission's radar blips (on a hostile person or a car with one in
//    it) come first; with none, every hostile ped (IsHostile: gangs, criminals,
//    police when wanted, attackers) and every car with a hostile in it
//  - the one nearest the heli's nose is taken first (angle, then distance)
// Firing: a Hunter shot counted in the last 0.3 s (the rocket / cannon hooks),
// or a fire key held: vehicle fire, handbrake / Space, or the left mouse.
static bool HeliAimOn(const Settings& A) {
    return A.heliAimbot && IsHunterModel(PlayerVehicle());
}
void HeliAimTick(const Settings& A, void* me) {
    static DWORD lastHit = 0, shotAt = 0, logAt = 0;
    static int seenShots = 0, winShots = 0, winTaken = 0, winWalls = 0;
    DWORD now = GetTickCount();
    int shots = g_heliShots;
    if (shots != seenShots) { winShots += shots - seenShots; seenShots = shots; shotAt = now; }
    void* heli = PlayerVehicle();
    if (!IsHunterModel(heli)) { logAt = 0; winShots = winTaken = winWalls = 0; return; }
    if (!logAt) logAt = now;
    if (now - logAt >= 60000) {
        Log("HeliAim: %d shots counted, %d targets taken (%d behind walls)", winShots, winTaken, winWalls);
        logAt = now; winShots = winTaken = winWalls = 0;
    }
    if (!HeliAimOn(A) || g_menuOpen || at<short>(Pad0, 0xF0) != 0) return;
    bool keys = GameFocused() && ((GetAsyncKeyState(VK_SPACE) & 0x8000) || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
    bool firing = at<short>(Pad0, 0x22) != 0 || at<short>(Pad0, 0x0C) != 0 || keys || now - shotAt < 300;
    if (!firing) return;
    if (now - lastHit < 250) return;
    const float* hp = &at<float>(heli, ENT_POS);
    const float* hf = &at<float>(heli, ENT_FWD);
    float fl = sqrtf(hf[0] * hf[0] + hf[1] * hf[1] + hf[2] * hf[2]);
    if (fl < 0.001f) return;
    auto hostileIn = [&](void* car) {
        void** seats = &at<void*>(car, VEH_DRIVER);
        for (int i = 0; i < 9; i++) if (seats[i] && IsHostile(seats[i], me)) return true;
        return false;
    };
    // nearest the nose first (in 5 degree steps), then nearest
    void* best = nullptr; bool bestCar = false; int bestStep = 1 << 30; float bestD = 1e9f;
    auto consider = [&](void* e, bool car) {
        if (!e || e == heli || e == me) return;
        if (car && (VehicleWrecked(e) || !IsLiveVehicle(e))) return;
        const float* tp = &at<float>(e, ENT_POS);
        float dx = tp[0] - hp[0], dy = tp[1] - hp[1], dz = tp[2] - hp[2];
        if (dx * dx + dy * dy > 400.0f * 400.0f) return;                  // across the ground; any height
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        float dot = d > 0.01f ? (dx * hf[0] + dy * hf[1] + dz * hf[2]) / (d * fl) : 1.0f;
        int step = (int)(acosf(fmaxf(-1.0f, fminf(1.0f, dot))) / 0.0872665f);
        if (step > bestStep || (step == bestStep && d >= bestD)) return;
        best = e; bestCar = car; bestStep = step; bestD = d;
    };
    // a hostile person: on foot, or the live car they sit in
    auto considerPed = [&](void* p) {
        if (!IsHostile(p, me)) return;
        if (at<unsigned char>(p, PED_IN_VEHICLE)) {
            void* v = at<void*>(p, PED_MY_VEHICLE);
            if (v) consider(v, true);
            return;
        }
        consider(p, false);
    };
    for (auto& b : ActiveBlips()) {
        if (b.type != 1 && b.type != 2) continue;
        void* e = BlipEntity(b.index, b.type);
        if (!e) continue;
        if (b.type == 2) considerPed(e);
        else if (hostileIn(e)) consider(e, true);
    }
    if (!best) {                                                          // no vigilante target: everyone hostile around
        if (char* pool = (char*)*PedPool) {
            char* objs = *(char**)(pool + POOL_OBJECTS);
            unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
            int size = *(int*)(pool + POOL_SIZE);
            for (int i = 0; i < size; i++) {
                if (flags[i] & 0x80) continue;
                considerPed(objs + i * PED_POOL_STRIDE);
            }
        }
    }
    if (!best) return;
    lastHit = now;
    // through walls by design; the log counts how many had a building in between
    const float* tp = &at<float>(best, ENT_POS);
    float from[3] = { hp[0], hp[1], hp[2] - 1.0f };                     // the gun, under the nose
    float to[3] = { tp[0], tp[1], tp[2] + (bestCar ? 0.5f : 0.3f) };
    winTaken++;
    if (!LineOfSightClear(from, to, true, false, false, false, false, true, true)) winWalls++;
    if (bestCar) BlowUpVehicle(best, me);
    else {
        InflictDamage(best, me, 45 /* UNIDENTIFIED */, 1000.0f, 0, 0);
        at<float>(best, PED_ARMOUR) = 0.0f;
        at<float>(best, PED_HEALTH) = 0.0f;
    }
}

// ---- weapons: explosive / fire bullets, teleport / delete / vehicle guns ----------
static int RandomCarModel() {
    std::vector<int> cars;
    for (auto& v : VehicleList()) if (v.category == CAT_CARS) cars.push_back(v.model);
    return cars.empty() ? 141 : cars[rand() % cars.size()];
}
void DeleteCarWithPeople(void* v, void* me) {
    void** seats = &at<void*>(v, VEH_DRIVER);
    for (int k = 0; k < 9; k++) if (seats[k] == me) return;          // never with you in it
    for (int k = 0; k < 9; k++)
        if (seats[k] && IsLivePed(seats[k])) { void* p = seats[k]; Scm(0x9B).I(PedH(p)).Run(); }
    Scm(0xA6).I(CarH(v)).Run();
}
void ShotFxTick(const Settings& A, void* me) {
    static int seen = 0;
    static DWORD lastFx = 0;
    int shots = g_playerShots;
    bool fired = shots != seen;
    seen = shots;
    if (!fired || PlayerVehicle() || *GameMenuActive) return;
    if (!A.explosiveBullets && !A.fireBullets && !A.teleportGun && !A.deleteGun && !A.vehicleGun) return;
    // Melee swings go through CWeapon::Fire too (and so are counted): only a
    // gun's bullet or a launcher's shot sets these off, never a punch.
    {
        int slot = at<signed char>(me, PED_CUR_WEAPON);
        if (slot < 0 || slot >= WEAPON_SLOTS) return;
        char* wi = GetWeaponInfo(at<int>((char*)me + PED_WEAPONS + slot * WEAPON_SIZE, WEAP_TYPE));
        int fire = wi ? at<int>(wi, WI_FIRETYPE) : 0;
        if (fire != 1 && fire != 2) return;
    }
    DWORD now = GetTickCount();
    if (now - lastFx < 60) return;                                       // rapid fire: not 22 blasts a second
    lastFx = now;
    float src[3], dir[3];
    if (!CrosshairRay(me, src, dir)) return;
    // the ray starts just past the player (the camera is behind them)
    const float* mp = &at<float>(me, ENT_POS);
    float tp = (mp[0] - src[0]) * dir[0] + (mp[1] - src[1]) * dir[1] + (mp[2] - src[2]) * dir[2];
    float t0 = fmaxf(1.0f, tp + 1.0f);
    float a[3] = { src[0] + dir[0] * t0, src[1] + dir[1] * t0, src[2] + dir[2] * t0 };

    if (A.vehicleGun) {
        float pos[3] = { mp[0] + dir[0] * 5.0f, mp[1] + dir[1] * 5.0f, mp[2] + 0.8f };
        if (void* v = SpawnVehicleObj(RandomCarModel(), false)) {
            float l = sqrtf(dir[0] * dir[0] + dir[1] * dir[1]);
            float fx = l > 0.01f ? dir[0] / l : 0, fy = l > 0.01f ? dir[1] / l : 1;
            EntityRemove(v);
            float* r = &at<float>(v, ENT_RIGHT); r[0] = fy; r[1] = -fx; r[2] = 0;
            float* f = &at<float>(v, ENT_FWD); f[0] = fx; f[1] = fy; f[2] = 0;
            memcpy(&at<float>(v, ENT_POS), pos, 12);
            EntityAdd(v);
            float* mv = &at<float>(v, PHYS_MOVE);
            mv[0] = dir[0] * 2.0f; mv[1] = dir[1] * 2.0f; mv[2] = dir[2] * 2.0f + 0.1f;
        }
    }
    float b[3] = { src[0] + dir[0] * 300.0f, src[1] + dir[1] * 300.0f, src[2] + dir[2] * 300.0f };
    char col[0x40] = {}; void* ent = nullptr;
    if (!ProcessLineOfSight(a, b, col, &ent, true, true, true, true, false, false, false, true)) return;
    const float* pt = (const float*)col;
    if (A.explosiveBullets) Scm(0x20C).F(pt[0]).F(pt[1]).F(pt[2]).I(2).Run();          // rocket blast
    else if (A.fireBullets) Scm(0x20C).F(pt[0]).F(pt[1]).F(pt[2]).I(1).Run();          // molotov fire
    if (A.deleteGun && ent) {
        int type = at<unsigned char>(ent, ENT_STATUS) & 7;                 // 2 vehicle, 3 ped
        if (type == 3 && ent != me && IsLivePed(ent)) {
            if (at<unsigned char>(ent, PED_IN_VEHICLE) && at<void*>(ent, PED_MY_VEHICLE)) DeleteCarWithPeople(at<void*>(ent, PED_MY_VEHICLE), me);
            else Scm(0x9B).I(PedH(ent)).Run();
        } else if (type == 2 && IsLiveVehicle(ent)) DeleteCarWithPeople(ent, me);
    }
    if (A.teleportGun)
        TeleportTo(pt[0] - dir[0] * 1.2f, pt[1] - dir[1] * 1.2f, pt[2] + 1.0f, false);
}

// ---- super punch: one punch sends them flying ---------------------------------------
void SuperPunchTick(const Settings& A, void* me) {
    static bool wasDown = false;
    static DWORD punchAt = 0;
    bool down = NewMouseState[0] != 0;
    bool click = down && !wasDown;
    wasDown = down;
    if (!A.superPunch || PlayerVehicle() || g_menuOpen) { punchAt = 0; return; }
    int slot = at<signed char>(me, PED_CUR_WEAPON);
    if (slot < 0 || slot >= WEAPON_SLOTS) return;
    char* wi = GetWeaponInfo(at<int>((char*)me + PED_WEAPONS + slot * WEAPON_SIZE, WEAP_TYPE));
    if (!wi || at<int>(wi, WI_FIRETYPE) != 0) return;                    // fists / melee weapons
    DWORD now = GetTickCount();
    if (click) punchAt = now + 150;                                       // when the swing lands
    if (!punchAt || now < punchAt) return;
    punchAt = 0;
    const float* mp = &at<float>(me, ENT_POS);
    const float* f = &at<float>(me, ENT_FWD);
    char* pool = (char*)*PedPool; if (!pool) return;
    char* objs = *(char**)(pool + POOL_OBJECTS);
    unsigned char* fl = *(unsigned char**)(pool + POOL_FLAGS);
    int size = *(int*)(pool + POOL_SIZE);
    for (int i = 0; i < size; i++) {
        if (fl[i] & 0x80) continue;
        void* p = objs + i * PED_POOL_STRIDE;
        if (p == me || PedDying(p) || at<unsigned char>(p, PED_IN_VEHICLE)) continue;
        const float* pp = &at<float>(p, ENT_POS);
        float dx = pp[0] - mp[0], dy = pp[1] - mp[1];
        float d = sqrtf(dx * dx + dy * dy);
        if (d > 2.2f || fabsf(pp[2] - mp[2]) > 1.5f || (dx * f[0] + dy * f[1]) < d * 0.4f) continue;   // in front
        InflictDamage(p, me, 0, 40.0f, 0, 0);
        PedSetFall(p, 2000, 0x1B, true);
        float* mv = &at<float>(p, PHYS_MOVE);
        mv[0] = f[0] * 1.6f; mv[1] = f[1] * 1.6f; mv[2] = 0.6f;
    }
    // and cars in front get a shove
    if (char* vp = (char*)*VehiclePool) {
        char* vo = *(char**)(vp + POOL_OBJECTS);
        unsigned char* vf = *(unsigned char**)(vp + POOL_FLAGS);
        int vs = *(int*)(vp + POOL_SIZE);
        for (int i = 0; i < vs; i++) {
            if (vf[i] & 0x80) continue;
            void* v = vo + i * VEHICLE_POOL_STRIDE;
            const float* p = &at<float>(v, ENT_POS);
            float dx = p[0] - mp[0], dy = p[1] - mp[1];
            float d = sqrtf(dx * dx + dy * dy);
            if (d > 3.5f || fabsf(p[2] - mp[2]) > 2.0f || (dx * f[0] + dy * f[1]) < d * 0.4f) continue;
            float* mv = &at<float>(v, PHYS_MOVE);
            mv[0] += f[0] * 1.0f; mv[1] += f[1] * 1.0f; mv[2] += 0.25f;
        }
    }
}
