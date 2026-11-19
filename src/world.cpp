// World: time and weather, gravity, clock speed, bullet time, hidden radar,
// field of view, and the tick that runs the Menyoo-style extras.
#include "features_internal.h"

// ------------------------------------------------------------------ world
void SetTime(int h, int m) {
    SetGameClock((unsigned char)h, (unsigned char)m);
    S.frozenHour = h; S.frozenMinute = m;
}
void ForceWeather(int type) { ForceWeatherNow(type); }
void ReleaseWeather() { *ForcedWeather = -1; }

// ---- world: gravity, clock speed, bullet time, radar -------------------------------
static void WorldExtrasTick(const Settings& A, void* me) {
    // gravity (0.008 for every physical, the ped's own fall separately)
    static float basePhys = 0.0f, basePed = 0.0f, applied = 1.0f;
    if (basePhys == 0.0f) { basePhys = *PhysGravity; basePed = *PedGravity; }
    if (fabsf(A.gravity - applied) > 0.001f) {
        float g = fminf(fmaxf(A.gravity, 0.05f), 4.0f);
        *PhysGravity = basePhys * g; *PedGravity = basePed * g;
        applied = A.gravity;
    }
    // clock speed: real milliseconds per game minute (1000 = normal)
    static bool clockSet = false;
    if (fabsf(A.clockSpeed - 1.0f) > 0.01f) {
        *MsPerGameMinute = (unsigned)fmaxf(10.0f, 1000.0f / fmaxf(A.clockSpeed, 0.05f));
        clockSet = true;
    } else if (clockSet) { *MsPerGameMinute = 1000; clockSet = false; }
    // bullet time: slow motion while you aim on foot
    static bool slowed = false;
    bool aim = A.bulletTime && !PlayerVehicle() && NewMouseState[1] != 0 && !g_menuOpen && GameFocused();
    if (aim) { if (*TimeScale > 0.31f) *TimeScale = 0.3f; slowed = true; }
    else if (slowed) { *TimeScale = 1.0f; slowed = false; }
    // radar hidden (DISPLAY_RADAR 0581), put back now and then - missions turn it on
    static int radarState = -1;
    static DWORD radarNext = 0;
    int want = A.hideRadar ? 0 : 1;
    if (want != radarState || (want == 0 && GetTickCount() >= radarNext)) {
        if (want == 0 || radarState == 0) Scm(0x581).I(want).Run();
        radarState = want; radarNext = GetTickCount() + 2000;
    }
}

void ExtrasTick(const Settings& A, void* me) {
    PlayerExtrasTick(A, me);
    ShotFxTick(A, me);
    SuperPunchTick(A, me);
    VehicleExtrasTick(A, me);
    BodyguardTick(me);
    WorldExtrasTick(A, me);
}

// ---- field of view ---------------------------------------------------------------
// The game's on-foot and driving cameras use 70 degrees. After the camera has
// run each frame (and before the frame is drawn) the render FOV is widened by
// the slider, so speed zoom-outs etc. scale along. The camera's own FOV is
// left alone, so its maths don't drift. The widening is by the tangent of the
// half angle, not the angle: VC Modern Fix stores the drawn FOV Hor+ (87.5 for
// a camera's 70 at 16:9), and scaling that by slider / 70 hit 150 (fish-eye) at
// slider 120; tan(drawn/2) * tan(slider/2) / tan(35) widens the view as much
// as the slider widens 70. Left out: scopes and other first-person views, and
// aiming when the crosshair hook isn't in (the shot's ray then uses the
// camera's own FOV, hooks.cpp HookFind3rdPersonCamTargetVector).
void FovTick(void* me) {
    static float written = -1.0f;
    float want = S.fov;
    if (!S.enabled || fabsf(want - 70.0f) < 0.05f || *GameMenuActive) return;
    if (*DrawFov == written) return;                                   // the camera didn't run: already done
    bool firstPerson = IsFirstPersonCamMode(at<short>(ActiveCam(), CAM_MODE));
    bool aiming = !g_wideCrosshairHook && !PlayerVehicle() && NewMouseState[1] != 0 && !g_menuOpen;
    if (firstPerson || aiming || !me) return;
    const float toRad = 3.14159265f / 180.0f;
    float t = tanf(*DrawFov * 0.5f * toRad) * tanf(want * 0.5f * toRad) / tanf(35.0f * toRad);
    float f = 2.0f * atanf(t) / toRad;
    *DrawFov = fminf(fmaxf(f, 20.0f), 150.0f);
    written = *DrawFov;
}
