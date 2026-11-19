// Trainer state and the per-tick driver: the settings, the action queue, the
// shared helpers, hotkeys and the settings file, the game's own cheats and the
// Active list, and TrainerTick, which runs every feature once per game update.
#include "features_internal.h"

Settings S;
static std::vector<std::function<void()>> g_queue;

DWORD g_killUntil = 0;    // suicide in progress: protections are suspended until then

DWORD g_blastSafeUntil = 0;  // just put out of an exploding vehicle

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

void MarkActiveDirty();
void SetTrainerEnabled(bool on) {
    S.enabled = on;
    MarkActiveDirty();
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

// A file in the game folder (data files, the trainer's own files), opened by
// full path so a different working directory can't hide it. A data file the
// game must have is logged once when it can't be opened.
FILE* OpenGameFile(const char* name, const char* mode, bool logMissing) {
    char path[MAX_PATH];
    GamePath(path, name);
    FILE* f = fopen(path, mode);
    if (!f && logMissing) Log("File: can't open %s (%s, errno %d)", path, mode, errno);
    return f;
}

// How full a pool is: its size and the slots in use (flag bit 0x80 = free).
void PoolUse(void* poolPtr, int* used, int* size) {
    *used = *size = 0;
    char* pool = (char*)poolPtr;
    if (!pool) return;
    unsigned char* flags = *(unsigned char**)(pool + POOL_FLAGS);
    *size = *(int*)(pool + POOL_SIZE);
    for (int i = 0; i < *size; i++) if (!(flags[i] & 0x80)) (*used)++;
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
    tog(P, "aim_silent", "Aim lock: silent aim", &S.aimSilent);
    tog(P, "aim_silent_bhop", "Aim lock: silent aim while bunny hopping", &S.aimSilentBhop);
    tog(P, "aim_snap_back", "Aim lock: snap camera back", &S.aimSnapBack);
    tog(P, "aim_drive_by", "Drive-by aimbot", &S.aimDriveBy);
    tog("Weapons", "wall_bang", "Wall bang (shoot through walls)", &S.wallBang);
    tog("Weapons", "rapid_fire", "Rapid fire", &S.rapidFire);
    tog("Weapons", "no_scope_sniper", "No-scope sniper", &S.noScopeSniper);
    tog("Weapons", "sniper_auto", "Sniper full auto", &S.sniperAuto);
    tog(P, "super_jump", "Super jump", &S.superJump);
    tog(P, "super_run", "Super run", &S.superRun);
    tog(P, "auto_bhop", "Auto bunny hop", &S.autoBhop);
    tog(P, "bhop_instant", "Bunny hop: instant full speed", &S.bhopInstant);
    tog(P, "bhop_keep_speed", "Bunny hop: keep speed on walls", &S.bhopKeepSpeed);
    tog(P, "no_landing_roll", "No landing roll", &S.noLandingRoll);
    tog("World", "genocide", "Genocide mode", &S.genocide);
    tog(P, "walk_on_water", "Walk on water", &S.walkOnWater);
    tog(P, "invisible", "Invisible", &S.invisible);
    tog(P, "freeze_position", "Freeze position (you or your vehicle)", &S.freezePos);
    tog(P, "forcefield", "Forcefield", &S.forcefield);
    tog(P, "super_punch", "Super punch", &S.superPunch);
    tog(P, "explosive_bullets", "Explosive bullets", &S.explosiveBullets);
    tog(P, "fire_bullets", "Fire bullets", &S.fireBullets);
    tog(P, "teleport_gun", "Teleport gun", &S.teleportGun);
    tog(P, "delete_gun", "Delete gun", &S.deleteGun);
    tog(P, "vehicle_gun", "Vehicle gun", &S.vehicleGun);
    press(P, "remove_weapons", "Remove all weapons", [] { RemoveAllWeapons(); });
    press(P, "bodyguards_dismiss", "Dismiss bodyguards", [] { DismissBodyguards(); });
    tog(P, "no_ragdoll", "No ragdoll (never knocked down)", &S.noRagdoll);
    tog("Weapons", "one_shot_vehicles", "One-shot vehicles", &S.oneShotVehicles);
    tog("Weapons", "bullet_penetration", "Bullet penetration (long range)", &S.bulletPenetrate);
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
    tog(V, "auto_flip", "Auto flip car / boat when upside down", &S.autoFlip);
    tog(V, "heli_aimbot", "Hunter aimbot (vigilante targets)", &S.heliAimbot);
    tog(V, "rainbow_car", "Rainbow paint", &S.rainbowCar);
    tog(V, "bulletproof_tyres", "Bulletproof tyres", &S.bulletproofTyres);
    tog(V, "invisible_car", "Invisible vehicle", &S.invisibleCar);
    press(V, "delete_vehicle", "Delete vehicle (yours or nearest)", [] { DeleteVehicle(); });
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
        static int w = 0; w = (w + 1) % 6; ForceWeather(w);
        const char* names[] = { "Sunny", "Cloudy", "Rain", "Fog", "Extra sunny", "Hurricane", "Extra colours" };
        ShowNative("Weather: %s", names[w]);
    });
    press(W, "weather_normal", "Normal weather", [] { ReleaseWeather(); });
    tog(W, "bullet_time", "Bullet time (slow-mo while aiming)", &S.bulletTime);
    tog(W, "hide_radar", "Hide radar", &S.hideRadar);
    press(W, "gravity_default", "Normal gravity", [] { S.gravity = 1.0f; ShowNative("Gravity: normal"); });
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
    tog(O, "overlay_showpos", "Show speed (vel)", &S.overlayShowPos);
    press(O, "fov_default", "Default field of view", [] { S.fov = 70.0f; ShowNative("Field of view: default"); });

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

bool KeyHeld(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

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
    {"enabled", &S.enabled}, {"stay_on_bike", &S.stayOnBike}, {"heavy_car", &S.heavyCar}, {"touch_kill", &S.touchKill}, {"aim_lock", &S.aimLock}, {"aim_silent", &S.aimSilent}, {"aim_silent_bhop", &S.aimSilentBhop}, {"aim_snap_back", &S.aimSnapBack}, {"aim_drive_by", &S.aimDriveBy}, {"wanted_cap", &S.wantedCapOn}, {"wall_bang", &S.wallBang}, {"rapid_fire", &S.rapidFire}, {"no_scope_sniper", &S.noScopeSniper}, {"sniper_auto", &S.sniperAuto}, {"super_jump", &S.superJump}, {"super_run", &S.superRun}, {"auto_bhop", &S.autoBhop}, {"bhop_surf", &S.bhopSurf}, {"bhop_instant", &S.bhopInstant}, {"bhop_keep_speed", &S.bhopKeepSpeed}, {"no_landing_roll", &S.noLandingRoll}, {"genocide", &S.genocide}, {"no_ragdoll", &S.noRagdoll}, {"walk_on_water", &S.walkOnWater}, {"one_shot_vehicles", &S.oneShotVehicles}, {"bullet_penetration", &S.bulletPenetrate}, {"enemy_esp", &S.enemyEsp}, {"auto_flip", &S.autoFlip}, {"heli_aimbot", &S.heliAimbot}, {"invisible", &S.invisible}, {"freeze_position", &S.freezePos}, {"forcefield", &S.forcefield}, {"explosive_bullets", &S.explosiveBullets}, {"fire_bullets", &S.fireBullets}, {"super_punch", &S.superPunch}, {"teleport_gun", &S.teleportGun}, {"delete_gun", &S.deleteGun}, {"vehicle_gun", &S.vehicleGun}, {"rainbow_car", &S.rainbowCar}, {"bulletproof_tyres", &S.bulletproofTyres}, {"invisible_car", &S.invisibleCar}, {"bullet_time", &S.bulletTime}, {"hide_radar", &S.hideRadar}, {"auto_lock", &S.autoLock}, {"survive_blowup", &S.surviveBlowUp},
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
    {"overlay_showpos", &S.overlayShowPos},
};
// Freeze time is not restored at startup on purpose: the frozen hour comes from
// the moment it was switched on, which a new session doesn't have.

static std::string BuildSettingsText();
static std::string g_savedText;     // what VCTrainer.ini holds now: saving the same again is skipped

void LoadSettings() {
    FILE* f = OpenGameFile(kIni, "r", false);
    if (!f) {
        Log("Settings: no VCTrainer.ini yet - defaults");
        g_savedText = BuildSettingsText();
        return;
    }
    char line[256];
    int keys = 0;
    while (fgets(line, sizeof line, f)) {
        char key[64]; char val[64];
        if (sscanf(line, " %63[^= ] = %63s", key, val) != 2) continue;
        keys++;
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
        if (!strcmp(key, "fov")) S.fov = fminf(fmaxf((float)atof(val), 40.0f), 120.0f);
        auto rng = [&](float lo, float hi) { return fminf(fmaxf((float)atof(val), lo), hi); };
        if (!strcmp(key, "damage_mult")) S.damageMult = rng(0.1f, 20.0f);
        if (!strcmp(key, "forcefield_mode")) S.forcefieldMode = atoi(val) == 1 ? 1 : 0;
        if (!strcmp(key, "car_accel")) S.carAccel = rng(1.0f, 5.0f);
        if (!strcmp(key, "car_grip")) S.carGrip = rng(1.0f, 5.0f);
        if (!strcmp(key, "car_brakes")) S.carBrakes = rng(1.0f, 5.0f);
        if (!strcmp(key, "gravity")) S.gravity = rng(0.1f, 3.0f);
        if (!strcmp(key, "clock_speed")) S.clockSpeed = rng(0.1f, 20.0f);
        if (!strcmp(key, "showpos_units")) { int v = atoi(val); S.showPosUnits = v < 0 || v > 2 ? 0 : v; }
        if (!strcmp(key, "super_run_speed")) S.superRunSpeed = fminf(fmaxf((float)atof(val), 1.2f), 5.0f);
        if (!strcmp(key, "heavy_mode")) S.heavyMode = atoi(val) % 3;
        if (!strcmp(key, "blowup_mode")) S.blowUpMode = atoi(val) == 1 ? 1 : 0;
        if (!strcmp(key, "max_health")) { int v = atoi(val); S.maxHealth = v >= 1 && v <= 255 ? v : 0; }
        if (!strcmp(key, "max_armour")) { int v = atoi(val); S.maxArmour = v >= 1 && v <= 255 ? v : 0; }
    }
    fclose(f);
    std::string on;
    int bound = 0;
    for (auto& b : kBools) if (*b.val && strcmp(b.key, "enabled") && strcmp(b.key, "warp_into_spawned")) { on += on.empty() ? "" : ", "; on += b.key; }
    for (auto& h : HotkeyActions()) if (h.vk) bound++;
    Log("Settings: loaded VCTrainer.ini, %d keys; trainer %s; %d hotkeys bound; on: %s", keys, S.enabled ? "ON" : "OFF",
        bound, on.empty() ? "nothing" : on.c_str());
    g_savedText = BuildSettingsText();
}

// The whole settings file as text.
static std::string BuildSettingsText() {
    std::string t;
    char b[256];
    auto add = [&](const char* fmt, auto... v) { snprintf(b, sizeof b, fmt, v...); t += b; };
    t += "# Modern VC Trainer settings (written by the trainer)\n";
    for (auto& k : kBools) add("%s = %d\n", k.key, *k.val ? 1 : 0);
    for (auto& h : HotkeyActions())
        if (h.vk) add("key_%s = %d\nkeymods_%s = %d\n", h.id.c_str(), h.vk, h.id.c_str(), h.mods);
    add("mouse_speed = %.2f\n", g_mouseSensitivity);
    add("fly_speed = %.1f\n", S.flySpeed);
    add("bhop_mode = %d\n", S.bhopMode);
    add("bhop_gain = %.2f\n", S.bhopGain);
    add("bhop_max = %.0f\n", S.bhopMax);
    add("bhop_height = %.1f\n", S.bhopHeight);
    add("wanted_cap_level = %d\n", S.wantedCap);
    add("super_jump_power = %.1f\n", S.superJumpPower);
    add("super_run_speed = %.1f\n", S.superRunSpeed);
    add("fov = %.0f\n", S.fov);
    add("forcefield_mode = %d\n", S.forcefieldMode);
    add("damage_mult = %.2f\ncar_accel = %.2f\ncar_grip = %.2f\ncar_brakes = %.2f\ngravity = %.2f\nclock_speed = %.2f\n",
        S.damageMult, S.carAccel, S.carGrip, S.carBrakes, S.gravity, S.clockSpeed);
    add("showpos_units = %d\n", S.showPosUnits);
    add("heavy_mode = %d\n", S.heavyMode);
    add("blowup_mode = %d\n", S.blowUpMode);
    if (S.maxHealth) add("max_health = %d\n", S.maxHealth);
    if (S.maxArmour) add("max_armour = %d\n", S.maxArmour);
    return t;
}

// Written only when something in it changed, and never in place: the new text
// goes to VCTrainer.ini.tmp, which then replaces the old file in one step, so
// a crash or a full disk mid-write can't leave an empty settings file.
void SaveSettings() {
    // Remember the max health/armour the player has right now, however it got
    // there (trainer slider, an earlier session, a save game).
    if (PlayerPed()) {
        if (*MaxHealth > 100) S.maxHealth = *MaxHealth;
        if (*MaxArmour > 100) S.maxArmour = *MaxArmour;
    }
    std::string text = BuildSettingsText();
    if (text == g_savedText) return;                             // nothing changed
    char path[MAX_PATH], tmp[MAX_PATH];
    GamePath(path, kIni);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE* f = fopen(tmp, "wb");
    if (!f) { Log("Settings: can't write %s (errno %d) - not saved", tmp, errno); return; }
    bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
    ok = fflush(f) == 0 && ok;
    ok = fclose(f) == 0 && ok;
    if (!ok || !MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        Log("Settings: saving VCTrainer.ini failed (write %s, error %lu) - the old file is kept", ok ? "ok" : "failed", GetLastError());
        DeleteFileA(tmp);
        return;
    }
    g_savedText = text;
}

bool OverlayEnabled() { return S.enabled && (S.overlayCoords || S.overlaySpeed || S.overlayFps || S.overlayActive || S.overlayShowPos); }
// The ImGui frame runs with the menu closed for the overlay and for the enemy
// markers (before, markers alone with no overlay row ticked drew nothing).
bool AnyOverlayOn() { return OverlayEnabled() || (S.enabled && S.enemyEsp); }

// A combo matches only with exactly its modifiers held, so "F" and "Shift + F"
// can be bound to different things.
static bool HotkeyDown(const HotkeyAction& h) {
    if (!h.vk || !KeyHeld(h.vk)) return false;
    return IsModifierKey(h.vk) || CurrentModifiers() == h.mods;
}

bool GameFocused() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

// Presses seen since the last tick: keys from the window procedure (key down,
// not auto-repeat), mouse buttons from a poll every drawn frame. Sampling the
// keys only on the 30 Hz tick missed taps shorter than a tick.
struct PendingPress { int vk, mods; };
static PendingPress g_pending[64];
static int g_nPending = 0;

void NoteKeyPress(int vk, int mods) {
    if (g_nPending < 64) g_pending[g_nPending++] = { vk, mods };
}

void PollMouseHotkeys() {
    static const int vks[5] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
    static bool was[5];
    bool focus = GameFocused();
    for (int i = 0; i < 5; i++) {
        bool d = focus && KeyHeld(vks[i]);
        if (d && !was[i]) NoteKeyPress(vks[i], CurrentModifiers());
        was[i] = d;
    }
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
        // A press is a recorded key-down with exactly this combo's modifiers
        // (a lone modifier key matches by itself); being held now isn't one,
        // so a press is never counted twice.
        bool pressed = false;
        if (active && allowed)
            for (int i = 0; i < g_nPending; i++)
                if (g_pending[i].vk == h.vk && (IsModifierKey(h.vk) || g_pending[i].mods == h.mods)) pressed = true;
        h.prev = down;
        switch (h.kind) {
        case HotkeyAction::Hold:   if (down || pressed) h.fn(); break;
        case HotkeyAction::Press:  if (pressed) h.fn(); break;
        case HotkeyAction::Toggle:
            if (pressed) { *h.toggle = !*h.toggle; MarkActiveDirty(); ShowNative("%s: %s", h.label.c_str(), *h.toggle ? "ON" : "OFF"); }
            break;
        }
    }
    g_nPending = 0;
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
    MarkActiveDirty();
    ShowNative("%s: %s", NativeCheatLabel(code), on ? "ON" : "OFF");
}

// The Active list is drawn every frame (sidebar count, Active page, overlay)
// but changes rarely, so it is built only when something it shows changed:
// MarkActiveDirty() from the trainer's own switches (hotkey toggles,
// SetNativeCheat, TurnEverythingOff, the master switch), and a small
// fingerprint of everything listed - each toggle, each game cheat flag,
// weather, speed, densities - which also catches menu checkboxes and changes
// the game makes itself (a cheat typed on the keyboard, a mission's slow motion).
static bool g_activeDirty = true;
void MarkActiveDirty() { g_activeDirty = true; }

static unsigned ActiveFingerprint() {
    unsigned h = 2166136261u;
    auto mix = [&](unsigned v) { h = (h ^ v) * 16777619u; };
    mix(S.enabled);
    for (auto& a : HotkeyActions()) if (a.kind == HotkeyAction::Toggle && a.toggle) mix(*a.toggle);
    bool ped = FindPlayerPed() != nullptr;
    mix(ped);
    if (ped) {
        for (auto& f : kCheatFlags) mix(NativeCheatOn(f.code));
        mix((unsigned)*ForcedWeather);
        mix((unsigned)(int)(*TimeScale * 100.0f + 0.5f));
        mix((unsigned)(int)(*PedDensity * 100.0f + 0.5f));
        mix((unsigned)(int)(*CarDensity * 100.0f + 0.5f));
    }
    return h;
}

static std::vector<ActiveItem> BuildActiveCheats();
const std::vector<ActiveItem>& ActiveCheats() {
    static std::vector<ActiveItem> cache;
    static unsigned seen = 0;
    unsigned now = ActiveFingerprint();
    if (g_activeDirty || now != seen) { cache = BuildActiveCheats(); seen = now; g_activeDirty = false; }
    return cache;
}

static std::vector<ActiveItem> BuildActiveCheats() {
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
    std::vector<ActiveItem> items = ActiveCheats();   // a copy: turning things off changes the list
    for (auto& a : items) a.turnOff();
    MarkActiveDirty();
    ShowNative("All cheats turned off");
}

// ------------------------------------------------------------------ per tick
void TrainerTick() {
    // During a replay every cheat pauses (menu actions wait in the queue):
    // the game's ped / vehicle / wanted data is the recording's, not live.
    if (ReplayPlaying()) { g_nPending = 0; g_espList.clear(); return; }
    const Settings& A = Active();
    // run what the menu asked for
    if (!g_queue.empty()) {
        auto q = std::move(g_queue);
        g_queue.clear();
        for (auto& fn : q) fn();
    }

    void* ped = PlayerPed();
    if (!ped) { g_nPending = 0; g_espList.clear(); return; }   // no presses kept for later

    ProcessHotkeys();
    FovTick(ped);
    ShowPosTick(ped);
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
    DoorLockRelock();
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
    WaterWalkTick(A, ped);
    BhopTick(A, ped);
    ShotTick(A, ped);
    DriveByAimTick(A, ped);
    RapidFireTick(A, ped);
    GenocideTick(A, ped);
    AutoFlipTick(A);
    HeliAimTick(A, ped);
    ExtrasTick(A, ped);
    StayOnBikeTick(A);
    BikeExitFix(A);
    FastExitTick(A);
    PendingWarpTick();
    WreckTick(A);
    CopCarEntryTick();
    SpawnedPedsTick();
    EnemyScanTick(A, ped);   // after the aim locks: it marks their targets

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
