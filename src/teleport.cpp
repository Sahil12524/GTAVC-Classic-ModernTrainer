// Teleport: saved locations, the free landing spot (no walls, vehicles,
// people or water), radar blips and pickups, moving targets and the
// mission-marker hotkey.
#include "features_internal.h"

// ------------------------------------------------------------------ teleport
static std::vector<SavedPos> g_saved;
static bool g_savedLoaded = false;
static const char* kSaveFile = "VCTrainer_positions.txt";

std::vector<SavedPos>& SavedPositions() {
    if (!g_savedLoaded) {
        g_savedLoaded = true;
        if (FILE* f = OpenGameFile(kSaveFile, "r", false)) {
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
    if (FILE* f = OpenGameFile(kSaveFile, "w")) {
        for (auto& p : g_saved) fprintf(f, "%.3f %.3f %.3f %s\n", p.x, p.y, p.z, p.name.c_str());
        fclose(f);
    } else ShowNative("Couldn't save the locations - see VCTrainer.log");
}

// ---- free landing spot ------------------------------------------------------
// Before any teleport the landing spot is checked against the real world:
// walls, fences, lamp posts (line-of-sight tests across the footprint) and
// other vehicles and people (their boxes). If the spot is taken, the nearest
// free spot around it is used instead, on the same ground and reachable
// from the original spot without passing through a wall.

Footprint EntityFootprint(void* e, bool vehicle) {
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
// Fly mode holds the player (or vehicle) where it flies, so a jump would be
// undone at once: every teleport is refused with a message instead.
static bool FlyBlocksTeleport() {
    if (!Active().flyMode) return false;
    ShowNative("Can't teleport while fly mode is on");
    return true;
}

void TeleportTo(float x, float y, float z, bool findGround, bool clearSpace, void* ignore, const float* heading) {
    if (FlyBlocksTeleport()) return;
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
void* BlipEntity(int index, int type) {
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
    if (FlyBlocksTeleport()) return;
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
    if (FlyBlocksTeleport()) return false;
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
