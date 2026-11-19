// Movement: super jump and super run, the CS:GO-style bunny hop and surf, no
// landing roll, walk on water, and fly mode (with its weapons).
#include "features_internal.h"

// ---- super jump / super run -----------------------------------------------------
// Super jump: the moment a jump starts (CPed state -> PED_JUMP), the upward
// speed is multiplied, plus a little more forward push. Super run: every
// frame on foot, the distance the player actually moved is stretched - so
// walking, running and sprinting all get faster - unless a wall is in the way.
void SuperMoveTick(const Settings& A, void* me) {
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
// A knock-down from the game's own car exit (rolling out of a moving car,
// being pulled out - the SetFall hook lets those through) must play out: its
// get-up is what stands the player up again. Killing it left them flat.
static DWORD g_carExitFallUntil = 0;
void NoteCarExitFall() { g_carExitFallUntil = GetTickCount() + 8000; }

static bool CancelLandingAnims(void* ped, bool alsoJumpLand) {
    if (g_carExitFallUntil && GetTickCount() < g_carExitFallUntil) return false;
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

void BhopTick(const Settings& A, void* me) {
    static float vel[3] = { 0, 0, 0 };                                    // m/s, the trainer's own movement
    static bool chain = false, wasStanding = true, surfing = false;
    static float hopAge = 1.0f;                                            // s since the last hop
    static bool justHopped = false;
    static LARGE_INTEGER freq, last;
    LARGE_INTEGER nowc; QueryPerformanceCounter(&nowc);
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    float dt = last.QuadPart ? (float)(nowc.QuadPart - last.QuadPart) / (float)freq.QuadPart : 0.0f;
    last = nowc;
    if (dt > 0.05f) dt = 0.05f;

    struct Pub { bool& c; ~Pub() { g_bhopping = c; } } pub{ chain };
    if (PlayerVehicle() || *GameMenuActive || g_menuOpen || !GameFocused()) {
        // Hop cut short (trainer menu, pause menu, alt-tab, into a car): the
        // take-off / in-air poses it holds at full weight must go too, or
        // they stay on for good and freeze the body mid-jump.
        if (chain || surfing) {
            if (PlayerVehicle()) { KillAnim(me, ANIM_JUMP_LAUNCH); KillAnim(me, ANIM_JUMP_GLIDE); }
            else EndHop(me);
        }
        chain = surfing = false; return;
    }
    bool standing = (at<unsigned char>(me, PED_FLAGS_14C) & 1) != 0;
    int st = at<int>(me, PED_STATE);
    float* mv = &at<float>(me, PHYS_MOVE);
    // Jump held: the pad's Square (+0x1C) - but in the first-person weapon
    // views the game's controls also put the mouse wheel's ZOOM IN there
    // (0x618BF5: action 7 -> mov word [ebp+1Ch],0FFh; CPad::SniperZoomIn
    // 0x4A9E90 reads it), and the game itself never jumps in a scope. So
    // Square doesn't count in a scope / first-person view, with a scope
    // weapon up on the aim button, or when the wheel turned this tick.
    bool held = at<short>(Pad0, PAD_JUMP) != 0;
    if (held) {
        int slot = at<signed char>(me, PED_CUR_WEAPON);
        int type = slot >= 0 && slot < WEAPON_SLOTS ? at<int>((char*)me + PED_WEAPONS + slot * WEAPON_SIZE, WEAP_TYPE) : 0;
        bool zoom = FirstPersonWeaponCam() || (IsScopeWeapon(type) && NewMouseState[1]) ||
                    NewMouseState[MOUSE_WHEEL_UP] || NewMouseState[MOUSE_WHEEL_DOWN];
        if (zoom) {
            held = false;
            static int ignored = 0;
            if (ignored++ == 0) Log("Bhop: wheel zoom ignored (jump read in a scope / first-person view or with the wheel turning)");
        }
    }

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
        float wl = 0.0f;                                                   // walk on water: the surface is ground
        if (A.walkOnWater && WaterLevelAt(pp[0], pp[1], pp[2], &wl) && (!f || wl > g)) { g = wl; f = true; }
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
        if (A.bhopInstant) {                                                // full speed from the first hop
            float dx = vel[0], dy = vel[1];
            if (sp < 1.0f) { dx = A.bhopMode == 2 ? -fx : fx; dy = A.bhopMode == 2 ? -fy : fy; sp = 1.0f; }
            vel[0] = dx / sp * A.bhopMax; vel[1] = dy / sp * A.bhopMax;
        }
        chain = true;
        InstantHop(A, me);
        hopAge = 0.0f; justHopped = true;
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
            // Each hop is a visible jump, not one held glide pose: it
            // kicks off with the push-up half of the take-off animation
            // (JUMP_launch from its middle, legs driving up) for a moment,
            // then cross-fades into the in-air pose, restarted from its
            // start, until the next hop. Surfing holds the in-air pose.
            const float kKick = 0.20f, kFade = 0.10f;
            char* g = GetAnimAssoc(clump, ANIM_JUMP_GLIDE);
            if (!g) g = (char*)BlendAnimation(clump, 0, ANIM_JUMP_GLIDE, 1000.0f);
            char* l = GetAnimAssoc(clump, ANIM_JUMP_LAUNCH);
            if (justHopped && !surfing) {
                if (!l) l = (char*)BlendAnimation(clump, 0, ANIM_JUMP_LAUNCH, 1000.0f);
                if (l) {
                    char* hier = at<char*>(l, 0x14);                      // CAnimBlendHierarchy
                    float len = hier ? at<float>(hier, 0x20) : 0.0f;      // totalLength
                    at<float>(l, 0x20) = (len > 0.05f && len < 5.0f) ? len * 0.5f : 0.2f;   // current time
                    at<float>(l, 0x24) = 1.0f;                            // speed
                    at<unsigned short>(l, ASSOC_FLAGS) |= 1;              // running
                    at<unsigned short>(l, ASSOC_FLAGS) &= ~ASSOCF_DELETEFADEDOUT;
                }
                if (g) at<float>(g, 0x20) = 0.0f;                         // in-air pose from its start
            }
            justHopped = false;
            float kick = 0.0f;                                            // take-off weight
            if (!surfing && l) {
                if (hopAge < kKick) kick = 1.0f;
                else if (hopAge < kKick + kFade) kick = 1.0f - (hopAge - kKick) / kFade;
            }
            if (l) {
                if (kick > 0.0f) { at<float>(l, ASSOC_BLEND) = kick; at<float>(l, ASSOC_DELTA) = 0.0f; }
                else KillAnim(me, ANIM_JUMP_LAUNCH);
            }
            if (g) { at<float>(g, ASSOC_BLEND) = 1.0f - kick; at<float>(g, ASSOC_DELTA) = 0.0f; }
            for (int a = 0; a <= 2; a++) KillAnim(me, a);                 // walk / run / sprint
            KillAnim(me, ANIM_IDLE_STANCE);
            KillAnim(me, ANIM_JUMP_LAND);
            KillAnim(me, ANIM_FALL_LAND);
            KillAnim(me, ANIM_FALL_COLLAPSE);
        }
        if (at<int>(me, PED_STATE) != PED_JUMP && (!standing || surfing)) at<int>(me, PED_STATE) = PED_JUMP;
        // Face where the camera looks, like a CS:GO player model: the game
        // only turns the ped while it handles walking, which the hop takes
        // over, so in the air the body stayed facing the take-off direction.
        // Heading: CPed +0x374 current, +0x378 target (radians, 0 = north);
        // the matrix is turned too so it shows this frame.
        float h = atan2f(-fx, fy);
        at<float>(me, 0x374) = h;
        at<float>(me, 0x378) = h;
        float* rgt = &at<float>(me, ENT_RIGHT);
        float* fwd = &at<float>(me, ENT_FWD);
        rgt[0] = fy; rgt[1] = -fx; rgt[2] = 0.0f;
        fwd[0] = fx; fwd[1] = fy; fwd[2] = 0.0f;
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

    hopAge += dt;
    if ((chain || surfing) && dt > 0.0f) {
        // Air strafing, CS:GO numbers: W/S/A/D give a wish direction relative
        // to the camera; sv_airaccelerate 12, wish speed 250 u/s (6.35 m/s)
        // capped at 30 u/s (0.76 m/s) in the air.
        float wf = (KeyHeld('W') ? 1.0f : 0.0f) - (KeyHeld('S') ? 1.0f : 0.0f);
        float ws = (KeyHeld('D') ? 1.0f : 0.0f) - (KeyHeld('A') ? 1.0f : 0.0f);
        float wx = fx * wf + rx * ws, wy = fy * wf + ry * ws;
        float wl = sqrtf(wx * wx + wy * wy);
        // Source's AirAccelerate, run on CS:GO's fixed 64 tick - not once per
        // rendered frame. Per frame, the gain from a strafe depended on the
        // frame rate and was only 35-65% of CS:GO's; per tick it is the same
        // as CS:GO at any FPS: each tick the wish speed (capped at 30 u/s)
        // minus the speed already going that way is added, at most
        // airaccelerate x 250 u/s x tick. Turning the mouse with A / D keeps
        // the wish direction across the run and the speed grows.
        static float tickAcc = 0.0f;
        const float tick = 1.0f / 64.0f;
        tickAcc = fminf(tickAcc + dt, 0.25f);
        bool inAir = wl > 0.01f && (!standing || surfing);
        if (inAir) { wx /= wl; wy /= wl; }
        for (; tickAcc >= tick; tickAcc -= tick) {
            if (!inAir) continue;
            const float wishCap = 0.762f, airAccel = 12.0f * 6.35f;
            float cur = vel[0] * wx + vel[1] * wy;
            float add = wishCap - cur;
            if (add > 0.0f) {
                float acc = fminf(airAccel * tick, add);
                vel[0] += wx * acc; vel[1] += wy * acc;
            }
        }
        // Instant full speed: the speed stays at the top speed the whole
        // chain. W turns the hop to where the camera looks, S to the opposite
        // way (quickly, not instantly); A / D are left to normal air
        // strafing. Nothing held: it keeps going the way it goes.
        if (A.bhopInstant && !surfing) {
            float sp = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
            float dx = sp > 0.5f ? vel[0] / sp : (A.bhopMode == 2 ? -fx : fx);
            float dy = sp > 0.5f ? vel[1] / sp : (A.bhopMode == 2 ? -fy : fy);
            if (wf != 0.0f) {
                float tx = fx * wf, ty = fy * wf;
                float k = fminf(1.0f, 10.0f * dt);
                dx += (tx - dx) * k; dy += (ty - dy) * k;
                float l = sqrtf(dx * dx + dy * dy);
                if (l > 0.01f) { dx /= l; dy /= l; } else { dx = tx; dy = ty; }
            }
            vel[0] = dx * A.bhopMax; vel[1] = dy * A.bhopMax;
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
            // Stairs, kerbs, low ledges: like Source's step-up, anything up to
            // 0.8 m above the feet just ahead lifts the player onto it instead
            // of stopping the hop (the feet are ~1 m below the ped's centre).
            auto stepUp = [&](float dx, float dy, float d) -> bool {
                const float feet = pos[2] - 1.0f, stepH = 0.8f;
                float ax = pos[0] + dx + dx / d * 0.3f, ay = pos[1] + dy + dy / d * 0.3f;
                bool f = false;
                float g = FindGroundZ3D(ax, ay, feet + stepH + 0.05f, &f);   // the top of the step
                if (!f) return false;
                float rise = g - feet;
                if (rise < 0.03f || rise > stepH) return false;
                float lift = rise + 0.04f;
                float a2[3] = { pos[0], pos[1], pos[2] + lift + 0.1f };        // room above the step?
                float b2[3] = { pos[0] + dx + dx / d * 0.45f, pos[1] + dy + dy / d * 0.45f, pos[2] + lift + 0.1f };
                float hp2[3], hn2[3];
                if (RayHit(a2, b2, hp2, hn2)) return false;                    // a real wall
                EntityRemove(me);
                pos[0] += dx; pos[1] += dy; pos[2] += lift;
                EntityAdd(me);
                if (mv[2] < 0.0f) mv[2] = 0.0f;                                // don't fall back into it
                return true;
            };
            // Slopes too gentle to surf (stunt ramps, banks, hills): ride up
            // the surface at full speed, and keep the climb as upward speed
            // so the top of a stunt ramp throws you into the air. Looks at
            // objects as well as buildings (spawned ramps are objects).
            auto rideSlope = [&](float dx, float dy, float d) -> bool {
                const float feet = pos[2] - 1.0f;
                float maxRise = d * 2.8f + 0.3f;                                // up to ~70 degrees
                float ax = pos[0] + dx + dx / d * 0.3f, ay = pos[1] + dy + dy / d * 0.3f;
                float top[3] = { ax, ay, feet + maxRise + 0.1f }, bot[3] = { ax, ay, feet - 0.6f };
                float gp[3], gn[3];
                if (!RayHit(top, bot, gp, gn) || gn[2] < 0.3f) return false;   // no walkable surface there
                float rise = gp[2] - feet;
                if (rise > maxRise) return false;
                float lift = fmaxf(rise, 0.0f) + 0.04f;
                float a2[3] = { pos[0], pos[1], pos[2] + lift + 0.1f };        // room to get there?
                float b2[3] = { pos[0] + dx + dx / d * 0.45f, pos[1] + dy + dy / d * 0.45f, pos[2] + lift + 0.1f };
                float hp2[3], hn2[3];
                if (RayHit(a2, b2, hp2, hn2) && hn2[2] < 0.3f) return false;   // a wall on the ramp
                EntityRemove(me);
                pos[0] += dx; pos[1] += dy; pos[2] += lift;
                EntityAdd(me);
                float vz = fminf(lift / dt, 40.0f) / 50.0f;                    // climb speed, game units
                if (mv[2] < vz) mv[2] = vz;
                return true;
            };
            for (int tries = 0; tries < 2; tries++) {
                float dx = vel[0] * dt, dy = vel[1] * dt;
                float d = sqrtf(dx * dx + dy * dy);
                if (d < 0.0001f) break;
                float a[3] = { pos[0], pos[1], pos[2] + 0.1f };
                float b[3] = { pos[0] + dx + dx / d * 0.45f, pos[1] + dy + dy / d * 0.45f, pos[2] + 0.1f };
                float hp[3], hn[3];
                if (!RayHit(a, b, hp, hn)) {
                    // body clear - a step at the feet just ahead?
                    float fa[3] = { pos[0], pos[1], pos[2] - 0.75f };
                    float fb[3] = { b[0], b[1], pos[2] - 0.75f };
                    float fp[3], fn[3];
                    if (RayHit(fa, fb, fp, fn) && (fn[2] > 0.25f ? rideSlope(dx, dy, d) : stepUp(dx, dy, d))) break;
                    EntityRemove(me);
                    pos[0] += dx; pos[1] += dy;
                    EntityAdd(me);
                    break;
                }
                if (hn[2] > 0.25f && rideSlope(dx, dy, d)) break;              // a ramp face, not a wall
                if (stepUp(dx, dy, d)) break;                                  // tall step / steep stairs
                // a wall: slide along it like Source does, not a dead stop
                float n2[3] = { hn[0], hn[1], 0.0f };
                float nl = sqrtf(n2[0] * n2[0] + n2[1] * n2[1]);
                float before = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
                if (nl < 0.05f) { if (!A.bhopKeepSpeed) vel[0] = vel[1] = 0.0f; break; }
                n2[0] /= nl; n2[1] /= nl;
                if (A.bhopKeepSpeed) {
                    // keep every bit of speed: slide along the wall at the
                    // same speed - head-on too (the way the camera leans
                    // decides which way), never bouncing back
                    float tx = -n2[1], ty = n2[0];                             // along the wall
                    float side = vel[0] * tx + vel[1] * ty;
                    if (fabsf(side) < before * 0.05f) side = fx * tx + fy * ty;
                    if (side < 0.0f) { tx = -tx; ty = -ty; }
                    vel[0] = tx * before; vel[1] = ty * before;
                } else ClipVel(vel, n2, 2);
            }
        }
    }
    wasStanding = standing;
}

// ---- walk on water --------------------------------------------------------------
// On foot, the water surface is solid ground. Vice City has no swimming:
// a ped in water drowns. Every frame, if the feet are at or below the water
// (and there's no real ground at or above it - piers, beaches and boats stay
// as they are), the player is put on the surface, stopped from sinking and
// marked as standing (CPed flags 0x14C bits 0-1). The next frame the game then
// walks / runs / sprints / jumps from there exactly like on land. Falling in
// from any height lands softly; being under water already pops you up.
// Landing: the game ends a jump or fall when its collision finds ground, and
// water has none - the ped would stay in the jump / fall pose (and its state)
// for good. So the trainer lands it itself: the moment the feet come down
// onto the surface from the air, the in-air and landing animations go and
// the ped is idle again (unless a bunny hop chain is running: it does its own).
// Standing on the water, a fall animation the game starts is cleared too, and
// a jump that stays stuck on the surface for a moment is ended.
constexpr int ANIM_FALL_FALL = 0x90, ANIM_FALL_GLIDE = 0x91;
static void LandOnWater(void* me) {
    EndHop(me);
    KillAnim(me, ANIM_FALL_FALL);
    KillAnim(me, ANIM_FALL_GLIDE);
}
void WaterWalkTick(const Settings& A, void* me) {
    static bool airborne = false;                                      // last frame: above the water, off the ground
    static DWORD stuckSince = 0;
    g_onWater = g_overWater = false;
    if (!A.walkOnWater || PlayerVehicle() || g_flyingOnFoot) { airborne = false; return; }
    int st = at<int>(me, PED_STATE);
    if (st == PED_DIE || st == PED_DEAD) { airborne = false; return; }
    float* pos = &at<float>(me, ENT_POS);
    float* mv = &at<float>(me, PHYS_MOVE);
    float wl = 0.0f;
    if (!WaterLevelAt(pos[0], pos[1], pos[2], &wl)) { airborne = false; return; }   // no water here
    bool f = false;
    float g = FindGroundZ3D(pos[0], pos[1], pos[2] + 1.0f, &f);
    if (f && g >= wl - 0.05f) { airborne = false; return; }           // real ground at or above the water
    float feet = pos[2] - 1.0f;
    if (feet > wl + 0.08f) {                                           // above it: jumping or falling onto it
        airborne = true; stuckSince = 0;
        // The game measures a jump's height to the sea floor, far below, and
        // turns it into a long fall (fall animation, falling state). Within
        // 6 m of the water it's a jump onto ground: the fall is stopped
        // (SetFall is refused through g_overWater) and a jump keeps its pose.
        if (feet - wl < 6.0f && !g_bhopping) {
            g_overWater = true;
            void* clump = at<void*>(me, ENT_CLUMP);
            bool fell = KillAnim(me, ANIM_FALL_FALL) | KillAnim(me, ANIM_FALL_GLIDE);
            if (fell || st == PED_FALL) {
                if (clump && !GetAnimAssoc(clump, ANIM_JUMP_GLIDE)) BlendAnimation(clump, 0, ANIM_JUMP_GLIDE, 16.0f);
                if (st == PED_FALL) at<int>(me, PED_STATE) = PED_JUMP;
            }
        }
        return;
    }
    pos[2] = wl + 1.02f;                                               // feet on the surface
    if (mv[2] < 0.0f) mv[2] = 0.0f;
    at<unsigned char>(me, PED_FLAGS_14C) |= 0x03;                      // standing, was standing
    g_onWater = true;
    bool landed = airborne;
    airborne = false;
    if (g_bhopping) { stuckSince = 0; return; }                        // the hop chain lands itself
    if (landed) { LandOnWater(me); stuckSince = 0; return; }
    void* clump = at<void*>(me, ENT_CLUMP);
    bool fallAnim = clump && (GetAnimAssoc(clump, ANIM_FALL_FALL) || GetAnimAssoc(clump, ANIM_FALL_GLIDE));
    if (st == PED_FALL || st == PED_GETUP || fallAnim) { LandOnWater(me); CancelLandingAnims(me, false); }
    // a jump that never left the surface (or came back without us seeing it
    // airborne): gliding yet not rising, on the surface, for 0.15 s
    bool gliding = clump && GetAnimAssoc(clump, ANIM_JUMP_GLIDE);
    if (st == PED_JUMP && gliding && mv[2] <= 0.0f) {
        DWORD now = GetTickCount();
        if (!stuckSince) stuckSince = now;
        else if (now - stuckSince > 150) { LandOnWater(me); stuckSince = 0; }
    } else stuckSince = 0;
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
void PushProjectilesOut(void* ped) {
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

void FlyTick() {
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
