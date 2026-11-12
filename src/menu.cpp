// The ImGui menu. Everything that touches game objects is pushed onto the
// action queue (Queue) and runs at the start of the next game frame.
#include "features.h"
#include "trainer.h"
#include "game.h"
#include "imgui.h"
#include "imgui_impl_dx8.h"
#include "tables.h"
#include <algorithm>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <windows.h>

// ------------------------------------------------------------------ style
void ApplyTrainerStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImGui::StyleColorsDark(&s);
    s.WindowRounding = 8; s.FrameRounding = 5; s.GrabRounding = 5; s.TabRounding = 5;
    s.ChildRounding = 6; s.PopupRounding = 6; s.ScrollbarRounding = 6;
    s.WindowPadding = ImVec2(12, 10); s.FramePadding = ImVec2(8, 5); s.ItemSpacing = ImVec2(8, 6);
    s.WindowBorderSize = 1; s.FrameBorderSize = 0;

    const ImVec4 pink(0.93f, 0.27f, 0.62f, 1.00f), pinkHi(1.00f, 0.42f, 0.74f, 1.00f);
    const ImVec4 teal(0.10f, 0.74f, 0.80f, 1.00f);
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]         = ImVec4(0.07f, 0.06f, 0.10f, 0.96f);
    c[ImGuiCol_ChildBg]          = ImVec4(0.10f, 0.09f, 0.14f, 1.00f);
    c[ImGuiCol_PopupBg]          = ImVec4(0.09f, 0.08f, 0.13f, 0.98f);
    c[ImGuiCol_Border]           = ImVec4(0.93f, 0.27f, 0.62f, 0.35f);
    c[ImGuiCol_TitleBg]          = ImVec4(0.12f, 0.06f, 0.14f, 1.00f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(0.30f, 0.07f, 0.24f, 1.00f);
    c[ImGuiCol_FrameBg]          = ImVec4(0.16f, 0.14f, 0.22f, 1.00f);
    c[ImGuiCol_FrameBgHovered]   = ImVec4(0.24f, 0.18f, 0.30f, 1.00f);
    c[ImGuiCol_FrameBgActive]    = ImVec4(0.30f, 0.20f, 0.36f, 1.00f);
    c[ImGuiCol_Button]           = ImVec4(0.42f, 0.11f, 0.32f, 1.00f);
    c[ImGuiCol_ButtonHovered]    = pink;
    c[ImGuiCol_ButtonActive]     = pinkHi;
    c[ImGuiCol_Header]           = ImVec4(0.42f, 0.11f, 0.32f, 0.80f);
    c[ImGuiCol_HeaderHovered]    = pink;
    c[ImGuiCol_HeaderActive]     = pinkHi;
    c[ImGuiCol_Tab]              = ImVec4(0.18f, 0.10f, 0.20f, 1.00f);
    c[ImGuiCol_TabHovered]       = pink;
    c[ImGuiCol_TabSelected]      = ImVec4(0.62f, 0.15f, 0.45f, 1.00f);
    c[ImGuiCol_CheckMark]        = teal;
    c[ImGuiCol_SliderGrab]       = teal;
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.90f, 0.95f, 1.00f);
    c[ImGuiCol_Separator]        = ImVec4(0.93f, 0.27f, 0.62f, 0.30f);
    c[ImGuiCol_Text]             = ImVec4(0.95f, 0.93f, 0.97f, 1.00f);
    c[ImGuiCol_TextDisabled]     = ImVec4(0.58f, 0.54f, 0.64f, 1.00f);
}

static void Section(const char* title) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.10f, 0.74f, 0.80f, 1.0f), "%s", title);
    ImGui::Separator();
}

static void ProofsUI(const char* id, Proofs& p, bool vehicle) {
    ImGui::PushID(id);
    ImGui::Checkbox("Bulletproof", &p.bullet);   ImGui::SameLine(160);
    ImGui::Checkbox("Fireproof", &p.fire);       ImGui::SameLine(310);
    ImGui::Checkbox("Explosion proof", &p.explosion);
    ImGui::Checkbox(vehicle ? "Solid (collision proof)" : "Collision proof", &p.collision); ImGui::SameLine(vehicle ? 220 : 160);
    ImGui::Checkbox("Melee proof", &p.melee);
    ImGui::PopID();
}

// ------------------------------------------------------------------ tabs
static void PlayerTab() {
    void* ped = PlayerPed();
    if (!ped) { ImGui::TextDisabled("Player not available."); return; }
    using namespace game;

    Section("Cheats");
    ImGui::Checkbox("Infinite health", &S.godMode);      ImGui::SameLine(200);
    ImGui::Checkbox("Infinite armour", &S.infiniteArmour);
    ImGui::Checkbox("Never wanted", &S.neverWanted);     ImGui::SameLine(200);
    ImGui::Checkbox("Infinite ammo", &S.infiniteAmmo);   ImGui::SameLine(380);
    ImGui::Checkbox("No reload", &S.noReload);
    ImGui::Checkbox("Wall bang", &S.wallBang); ImGui::SameLine(200);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Your bullets go through walls, cars, wrecks and anything else:\n"
                          "whoever is on the crosshair line behind it gets hit.");
    ImGui::Checkbox("One-shot vehicles", &S.oneShotVehicles);
    ImGui::Checkbox("Rapid fire", &S.rapidFire);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Hold the fire button and any gun keeps shooting, about 22 shots a second -\n"
                          "pistols and shotguns too. Reloads still happen unless No reload is on.");
    ImGui::SameLine(200);
    ImGui::Checkbox("No-scope sniper", &S.noScopeSniper);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Sniper, laser sniper and rocket launcher without the scope / sight: one left click fires down your\n"
                          "crosshair (real bullet, sound, tracer) and kills whoever it hits.\n"
                          "With Aim lock-on, hold left click: the crosshair locks on, then it fires.\n"
                          "Right-click still scopes in.");
    ImGui::Checkbox("Sniper full auto", &S.sniperAuto);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Sniper, laser sniper and rocket launcher: hold the fire button and it keeps firing at full speed,\n"
                          "in the scope or (with No-scope sniper) from the hip - no tapping.");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Any vehicle you shoot or hit with a rocket blows up at once -\n"
                          "tanks, helicopters, armoured trucks, everything. Yours is never touched.");
    ImGui::Checkbox("Infinite sprint", &S.neverTired);   ImGui::SameLine(200);
    ImGui::Checkbox("Fast reload", &S.fastReload);       ImGui::SameLine(380);
    ImGui::Checkbox("Aim lock-on", &S.aimLock);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("While you aim (right mouse), the crosshair snaps onto the closest enemy\n"
                          "anywhere around you (360 degrees): every gang member, cop and criminal, and\n"
                          "anyone attacking you - even knocked to the ground. Your own followers are left alone.");
    ImGui::Checkbox("No ragdoll", &S.noRagdoll);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("You're never knocked down: cars, explosions, punches and falls\n"
                          "can still hurt you, but you stay on your feet.");
    ImGui::Checkbox("Show enemies through walls", &S.enemyEsp);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Marks every gang member, cop, criminal and attacker, in real time: a red box when you can see\n"
                          "them, a dashed orange box when they're behind a wall, the distance under it,\n"
                          "and arrows round the screen for the ones behind you or off to the side.");

    Section("Movement");
    ImGui::Checkbox("Super jump", &S.superJump); ImGui::SameLine(200);
    ImGui::SetNextItemWidth(200);
    ImGui::SliderFloat("Jump power", &S.superJumpPower, 1.5f, 8.0f, "%.1fx");
    ImGui::Checkbox("Super run", &S.superRun); ImGui::SameLine(200);
    ImGui::SetNextItemWidth(200);
    ImGui::SliderFloat("Run speed", &S.superRunSpeed, 1.2f, 5.0f, "%.1fx");
    ImGui::Checkbox("Auto bunny hop", &S.autoBhop);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Hold jump: you hop again the moment you land and keep your speed, like CS:GO.\n"
                          "Air strafe: hold A or D and turn the mouse the same way to curve and gain speed.\n"
                          "Every hop adds the gain (up to the top speed). Let go of jump to stop.");
    if (S.autoBhop) {
        ImGui::Indent();
        ImGui::RadioButton("Any direction", &S.bhopMode, 0); ImGui::SameLine();
        ImGui::RadioButton("AFH (forward)", &S.bhopMode, 1); ImGui::SameLine();
        ImGui::RadioButton("ABH (backward)", &S.bhopMode, 2);
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("Speed gain per hop", &S.bhopGain, 1.0f, 1.5f, "x%.2f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("Top hop speed", &S.bhopMax, 10.0f, 150.0f, "%.0f m/s");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("Hop height", &S.bhopHeight, 0.5f, 4.0f, "%.2f m");
        ImGui::Checkbox("Surf on steep ramps", &S.bhopSurf);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Like CS:GO surf: land on a ramp steeper than about 45 degrees and you slide along it\n"
                              "with no friction. Hold A or D into the ramp to stay on it, turn the mouse to steer.");
        ImGui::SameLine();
        if (ImGui::Button("CS:GO feel")) {
            S.bhopMode = 0; S.bhopGain = 1.0f; S.bhopHeight = 1.45f; S.bhopMax = 89.0f; S.bhopSurf = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("CS:GO's own numbers: speed only from air strafing (no gain per hop),\n"
                              "57-unit jump, 3500 u/s top speed, surf on.");
        ImGui::Unindent();
    }
    ImGui::Checkbox("No landing roll", &S.noLandingRoll);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Landing from a fall you don't roll forward, crouch or get up slowly -\n"
                          "you're straight back on your feet.");
    ImGui::TextDisabled("Big jumps can mean a hard landing - No ragdoll and Infinite health help.");

    Section("Fly mode (no clip)");
    ImGui::Checkbox("Fly", &S.flyMode);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::SliderFloat("Speed", &S.flySpeed, 5.0f, 150.0f, "%.0f m/s");
    ImGui::TextDisabled("W/S forward/back where the camera looks, A/D sideways, Space up, Ctrl down,\n"
                        "hold Shift for 3x speed. Goes through walls and works in a vehicle too.\n"
                        "Turning it off mid-air drops you - keep infinite health on.");

    Section("Proofs");
    ProofsUI("pp", S.playerProofs, false);

    Section("Health & armour");
    float maxH = (float)*MaxHealth, maxA = (float)*MaxArmour;
    float hp = at<float>(ped, PED_HEALTH), ar = at<float>(ped, PED_ARMOUR);
    ImGui::SetNextItemWidth(250);
    if (ImGui::SliderFloat("Health", &hp, 0.0f, maxH, "%.0f")) Queue([hp] { SetHealth(hp); });
    ImGui::SetNextItemWidth(250);
    if (ImGui::SliderFloat("Armour", &ar, 0.0f, maxA, "%.0f")) Queue([ar] { SetArmour(ar); });
    if (ImGui::Button("Fill health")) Queue([maxH] { SetHealth(maxH); }); ImGui::SameLine();
    if (ImGui::Button("Fill armour")) Queue([maxA] { SetArmour(maxA); });
    int mh = *MaxHealth, ma = *MaxArmour;
    ImGui::SetNextItemWidth(250);
    if (ImGui::SliderInt("Max health", &mh, 100, 250)) Queue([mh] { SetMaxHealth(mh); });
    ImGui::SetNextItemWidth(250);
    if (ImGui::SliderInt("Max armour", &ma, 100, 250)) Queue([ma] { SetMaxArmour(ma); });

    Section("Wanted level");
    int wanted = GetWanted();
    ImGui::SetNextItemWidth(250);
    if (ImGui::SliderInt("Stars", &wanted, 0, 6)) Queue([wanted] { SetWanted(wanted); });
    ImGui::SameLine();
    if (ImGui::Button("Clear")) Queue([] { SetWanted(0); });
    ImGui::Checkbox("Lock wanted level at most", &S.wantedCapOn);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("The wanted level can still go up, but never past the number of stars chosen here.\n"
                          "Never wanted (in Cheats) overrides it with zero.");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::SliderInt("stars##cap", &S.wantedCap, 0, 6);

    Section("Money");
    ImGui::Text("Cash: $%d", *Money);
    const int amounts[] = { 1000, 5000, 10000, 100000, 1000000 };
    const char* labels[] = { "+$1K", "+$5K", "+$10K", "+$100K", "+$1M" };
    for (int i = 0; i < 5; i++) {
        if (i) ImGui::SameLine();
        int a = amounts[i];
        if (ImGui::Button(labels[i])) Queue([a] { AddMoney(a); });
    }
    static int setTo = 10000;
    ImGui::SetNextItemWidth(160);
    ImGui::InputInt("##money", &setTo, 1000, 100000);
    ImGui::SameLine();
    if (ImGui::Button("Set money")) { int v = setTo; Queue([v] { SetMoney(v); }); }
    ImGui::SameLine();
    if (ImGui::Button("Take $10K")) Queue([] { AddMoney(-10000); });
}


static void WeaponsTab() {
    static int ammoOverride = 0;
    Section("Options");
    ImGui::Checkbox("Infinite ammo", &S.infiniteAmmo); ImGui::SameLine(200);
    ImGui::Checkbox("No reload", &S.noReload);
    ImGui::SetNextItemWidth(160);
    ImGui::InputInt("Ammo to give (0 = default)", &ammoOverride, 50, 500);
    if (ammoOverride < 0) ammoOverride = 0;

    Section("Weapon sets (game cheats)");
    if (ImGui::Button("Set 1 - Thug"))         Queue([] { RunNativeCheat("THUGSTOOLS"); });         ImGui::SameLine();
    if (ImGui::Button("Set 2 - Professional")) Queue([] { RunNativeCheat("PROFESSIONALTOOLS"); });  ImGui::SameLine();
    if (ImGui::Button("Set 3 - Nutter"))       Queue([] { RunNativeCheat("NUTTERTOOLS"); });

    Section("Give weapon");
    ImGui::BeginChild("weapons", ImVec2(0, 0), ImGuiChildFlags_None);
    for (auto& g : kWeapons) {
        ImGui::TextDisabled("%s", g.group);
        int n = 0;
        for (auto& w : g.w) {
            if (!w.type) break;
            if (n++ % 4) ImGui::SameLine();
            int type = w.type, ammo = ammoOverride ? ammoOverride : w.ammo;
            if (ImGui::Button(w.name, ImVec2(128, 0))) Queue([type, ammo] { GiveWeapon(type, ammo); });
        }
    }
    ImGui::EndChild();
}

static void VehicleTab() {
    void* veh = PlayerVehicle();
    Section("Current vehicle");
    if (!veh) {
        ImGui::TextDisabled("You are not in a vehicle. Options below still apply to the next one.");
        if (ImGui::Button("Get into nearest vehicle")) Queue([] { WarpIntoNearestVehicle(); });
    } else {
        ImGui::Text("Health: %.0f    Speed: %.0f km/h", game::at<float>(veh, game::VEH_HEALTH), VehicleSpeedKmh());
    }
    ImGui::BeginDisabled(!veh);
    if (ImGui::Button("Repair")) Queue([] { RepairVehicle(); });   ImGui::SameLine();
    if (ImGui::Button("Flip / upright")) Queue([] { FlipVehicle(); }); ImGui::SameLine();
    if (ImGui::Button("Lock doors")) Queue([] { SetDoorLock(true); }); ImGui::SameLine();
    if (ImGui::Button("Unlock doors")) Queue([] { SetDoorLock(false); }); ImGui::SameLine();
    ImGui::Checkbox("Auto lock doors", &S.autoLock);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Locks the doors as soon as you're sitting in a car, van, helicopter or plane,\n"
                          "and unlocks them when you get out. Bikes, boats and cars without doors are skipped.");
    if (ImGui::Button("Boost")) Queue([] { BoostVehicle(0.6f); });   ImGui::SameLine();
    if (ImGui::Button("Jump")) Queue([] { JumpVehicle(0.35f); });   ImGui::SameLine();
    if (ImGui::Button("Stop")) Queue([] { StopVehicle(); });
    ImGui::EndDisabled();
    ImGui::Checkbox("Car god mode (health)", &S.carGodMode); ImGui::SameLine(250);
    ImGui::Checkbox("Auto repair", &S.autoRepair);
    ImGui::Checkbox("Never fall off bike", &S.stayOnBike);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Crashes, hits and shots no longer throw you off a motorbike.");
    ImGui::SameLine(250);
    ImGui::Checkbox("Heavy car (like the Rhino)", &S.heavyCar);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Your car gets the Rhino tank's weight, so it shoves other cars aside.\n"
                          "Your car's normal weight comes back when you turn this off or get out.");
    ImGui::Checkbox("Survive vehicle explosions", &S.surviveBlowUp);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("If the car, bike, boat or helicopter you're in blows up, you don't die:\n"
                          "you stay in the wreck or are set down beside it, and the blast and fire\n"
                          "can't kill you while you're in or near it. Works without god mode.");
    if (S.surviveBlowUp) {
        ImGui::Indent();
        ImGui::RadioButton("Stay in the wreck", &S.blowUpMode, 0);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("You keep your seat in the burnt-out vehicle. Press exit to get out\n"
                              "(if the game won't let you, the trainer sets you down beside it).");
        ImGui::SameLine();
        ImGui::RadioButton("Thrown clear beside it", &S.blowUpMode, 1);
        ImGui::Unindent();
    }
    ImGui::Checkbox("Auto flip car", &S.autoFlip);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Cars only (not bikes). As soon as your car is on its roof on the ground,\n"
                          "it is turned back onto its wheels and keeps going. Never while it's in the air.");
    ImGui::Checkbox("Kill peds on one touch", &S.touchKill);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("While you drive any vehicle, a pedestrian it touches dies at once,\n"
                          "at any speed. People sitting in vehicles are left alone.");
    if (S.heavyCar) {
        ImGui::Indent();
        ImGui::TextDisabled("When you touch another car while driving:");
        ImGui::RadioButton("Just heavy", &S.heavyMode, 0);          ImGui::SameLine();
        ImGui::RadioButton("Push it far away", &S.heavyMode, 1);    ImGui::SameLine();
        ImGui::RadioButton("Blow it up", &S.heavyMode, 2);
        ImGui::Unindent();
    }
    ProofsUI("cp", S.carProofs, true);

    if (veh && ImGui::CollapsingHeader("Paint")) {
        int c1 = 0, c2 = 0;
        GetVehicleColours(c1, c2);
        static int which = 0;
        ImGui::RadioButton("Primary", &which, 0); ImGui::SameLine();
        ImGui::RadioButton("Secondary", &which, 1);
        const auto& pal = CarPalette();
        for (int i = 0; i < (int)pal.size(); i++) {
            if (i % 16) ImGui::SameLine(0, 3);
            ImVec4 col(pal[i].r / 255.0f, pal[i].g / 255.0f, pal[i].b / 255.0f, 1.0f);
            bool cur = (which ? c2 : c1) == i;
            ImGui::PushID(i);
            if (cur) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
            if (ImGui::ColorButton("##c", col, ImGuiColorEditFlags_NoTooltip, ImVec2(22, 22))) {
                int a = which ? c1 : i, b = which ? i : c2;
                Queue([a, b] { SetVehicleColours(a, b); });
            }
            if (cur) ImGui::PopStyleVar();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%d  %s", i, pal[i].name.c_str());
            ImGui::PopID();
        }
    }

    Section("Spawn vehicle");
    static int cat = 0, selected = -1;
    static char filter[32] = "";
    const char* cats[] = { "Cars", "Bikes", "Boats", "Helicopters", "RC" };
    for (int i = 0; i < CAT_COUNT; i++) {
        if (i) ImGui::SameLine();
        bool on = cat == i;
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
        if (ImGui::Button(cats[i])) { cat = i; selected = -1; }
        if (on) ImGui::PopStyleColor();
    }
    ImGui::SetNextItemWidth(220);
    ImGui::InputTextWithHint("##filter", "Search...", filter, sizeof filter);
    ImGui::SameLine();
    ImGui::BeginDisabled(selected < 0);
    if (ImGui::Button("Spawn", ImVec2(110, 0))) { int m = selected; Queue([m] { SpawnVehicle(m); }); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("Put me in it", &S.warpIntoSpawned);
    if (cat == CAT_BOATS) ImGui::TextDisabled("Boats spawn in front of you - stand near water.");

    ImGui::BeginChild("vehlist", ImVec2(0, 0), ImGuiChildFlags_Borders);
    for (auto& v : VehicleList()) {
        if (v.category != cat) continue;
        if (filter[0]) {
            std::string a = v.name, b = filter;
            for (auto& ch : a) ch = (char)tolower(ch);
            for (auto& ch : b) ch = (char)tolower(ch);
            if (a.find(b) == std::string::npos) continue;
        }
        ImGui::PushID(v.model);
        if (ImGui::Selectable(v.name.c_str(), selected == v.model, ImGuiSelectableFlags_AllowDoubleClick)) {
            selected = v.model;
            if (ImGui::IsMouseDoubleClicked(0)) { int m = v.model; Queue([m] { SpawnVehicle(m); }); }
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

static void WorldTab() {
    using namespace game;
    Section("Time");
    static int h = 12, m = 0;
    ImGui::Text("Game clock: %02d:%02d", *ClockHours, *ClockMinutes);
    ImGui::SetNextItemWidth(200); ImGui::SliderInt("Hour", &h, 0, 23);
    ImGui::SetNextItemWidth(200); ImGui::SliderInt("Minute", &m, 0, 59);
    if (ImGui::Button("Set time")) { int hh = h, mm = m; Queue([hh, mm] { SetTime(hh, mm); }); }
    ImGui::SameLine();
    if (ImGui::Checkbox("Freeze time", &S.freezeTime) && S.freezeTime) {
        S.frozenHour = *ClockHours; S.frozenMinute = *ClockMinutes;
    }

    Section("Weather");
    const char* weathers[] = { "Sunny", "Cloudy", "Rain", "Fog", "Extra sunny", "Hurricane", "Extra colours" };
    static int w = 0;
    ImGui::SetNextItemWidth(200); ImGui::Combo("##weather", &w, weathers, 7);
    ImGui::SameLine();
    if (ImGui::Button("Force weather")) { int ww = w; Queue([ww] { ForceWeather(ww); }); }
    ImGui::SameLine();
    if (ImGui::Button("Normal weather")) Queue([] { ReleaseWeather(); });

    Section("Game speed & population");
    float speed = *TimeScale;
    ImGui::SetNextItemWidth(250);
    if (ImGui::SliderFloat("Game speed", &speed, 0.1f, 3.0f, "%.2fx")) Queue([speed] { *TimeScale = speed; });
    ImGui::SameLine(); if (ImGui::Button("Reset##speed")) Queue([] { *TimeScale = 1.0f; });
    float peds = *PedDensity, cars = *CarDensity;
    ImGui::SetNextItemWidth(250);
    if (ImGui::SliderFloat("Pedestrians", &peds, 0.0f, 3.0f, "%.2fx")) Queue([peds] { *PedDensity = peds; });
    ImGui::SetNextItemWidth(250);
    if (ImGui::SliderFloat("Traffic", &cars, 0.0f, 3.0f, "%.2fx")) Queue([cars] { *CarDensity = cars; });

    Section("Chaos");
    ImGui::Checkbox("Genocide mode", &S.genocide);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("While on, every ped around you dies and every vehicle around you blows up,\n"
                          "including new ones that appear. You, your vehicle and your passengers are spared.\n"
                          "Explosions next to you still hurt - keep infinite health on.");

    Section("Missions");
    ImGui::Checkbox("Disable mission timers (countdown never runs out)", &S.freezeMissionTimer);
}

// ---- radar blips for the teleport tab ---------------------------------------
// The game's radar icons (CRadar::RadarSprites at 0x68F960, 40 CSprite2d*,
// each holding an RwTexture*). The Direct3D texture sits in the raster's
// driver data; it is looked for there and checked before it is ever drawn,
// so an unexpected renderer just means no icon, never a crash.
static ImTextureID RadarIcon(int sprite) {
    static ImTextureID cache[40];
    static bool tried[40];
    if (sprite <= 0 || sprite >= 40) return 0;
    if (tried[sprite]) return cache[sprite];
    tried[sprite] = true;
    void** spr = ((void***)0x68F960)[sprite];
    if (!spr || IsBadReadPtr(spr, 4)) return 0;
    char* tex = (char*)*spr;
    if (!tex || IsBadReadPtr(tex, 4)) return 0;
    char* raster = *(char**)tex;
    if (!raster || IsBadReadPtr(raster, 0x60)) return 0;
    for (int off = 0x28; off <= 0x5C; off += 4) {
        void* p = *(void**)(raster + off);
        if (ImGui_ImplDX8_IsTexture(p)) { cache[sprite] = (ImTextureID)(intptr_t)p; break; }
    }
    return cache[sprite];
}

// What each radar icon is, and which list of the Teleport tab it goes in.
enum { PC_MISSION, PC_EVENTS, PC_CONTACTS, PC_HOMES, PC_BUSINESS, PC_SHOPS, PC_OTHER, PC_COUNT };
struct SpriteInfo { const char* name; int cat; };
static SpriteInfo SpriteKind(int sprite) {
    static const SpriteInfo k[40] = {
        {"", PC_MISSION}, {"", PC_OTHER}, {"", PC_OTHER}, {"", PC_OTHER},
        {"Avery Carrington", PC_CONTACTS}, {"Mitch Baker", PC_CONTACTS}, {"Colonel Cortez", PC_CONTACTS},
        {"Ricardo Diaz", PC_CONTACTS}, {"Kent Paul", PC_CONTACTS}, {"Ken Rosenberg", PC_CONTACTS},
        {"Phil Cassidy", PC_CONTACTS}, {"Biker bar", PC_CONTACTS}, {"Boatyard", PC_BUSINESS},
        {"Malibu Club", PC_BUSINESS}, {"Umberto Robina", PC_CONTACTS}, {"InterGlobal Films", PC_BUSINESS},
        {"Ammu-Nation", PC_SHOPS}, {"Auntie Poulet", PC_CONTACTS}, {"Hardware store", PC_SHOPS},
        {"Safehouse", PC_HOMES}, {"Pole Position Club", PC_BUSINESS}, {"Cherry Popper", PC_BUSINESS},
        {"Kaufman Cabs", PC_BUSINESS}, {"Love Fist", PC_CONTACTS}, {"Print works", PC_BUSINESS},
        {"Property for sale", PC_HOMES}, {"Sunshine Autos", PC_BUSINESS}, {"Pay 'n' Spray", PC_SHOPS},
        {"Clothes shop", PC_SHOPS}, {"Vercetti Estate", PC_CONTACTS}, {"Payphone mission", PC_CONTACTS},
        {"Radio", PC_OTHER}, {"Radio", PC_OTHER}, {"Radio", PC_OTHER}, {"Radio", PC_OTHER},
        {"Radio", PC_OTHER}, {"Radio", PC_OTHER}, {"Radio", PC_OTHER}, {"Radio", PC_OTHER},
        {"Radio", PC_OTHER},
    };
    return sprite >= 0 && sprite < 40 ? k[sprite] : SpriteInfo{ "Map icon", PC_OTHER };
}

// The game's district boxes (data\navig.zon). The junkyard comes first as it
// sits inside the airport's box.
static const char* DistrictAt(float x, float y) {
    struct Z { const char* name; float x0, y0, x1, y1; };
    static const Z z[] = {
        { "Junkyard",              -1396.76f,  -42.91f, -1208.21f,  230.39f },
        { "Downtown",              -1613.03f,  413.13f,  -213.73f, 1677.32f },
        { "Vice Point",              163.66f, -351.15f,  1246.03f, 1398.85f },
        { "Washington Beach",       -103.97f, -930.53f,  1246.03f, -351.15f },
        { "Ocean Beach",            -253.21f,-1805.37f,  1254.90f, -930.53f },
        { "Escobar International", -1888.21f,-1779.61f, -1208.21f,  230.39f },
        { "Starfish Island",        -748.21f, -818.27f,  -104.51f, -241.47f },
        { "Prawn Island",           -213.73f,  797.61f,   163.66f, 1243.47f },
        { "Leaf Links",             -213.73f, -241.43f,   163.66f,  797.61f },
        { "Viceport",              -1208.21f,-1779.61f,  -253.21f, -898.74f },
        { "Little Havana",         -1208.21f, -898.74f,  -748.21f, -241.47f },
        { "Little Haiti",          -1208.21f, -241.47f,  -578.29f,  412.66f },
    };
    for (auto& e : z)
        if (x >= e.x0 && x <= e.x1 && y >= e.y0 && y <= e.y1) return e.name;
    return "";
}

// Fixed places, always listed (the radar shows only some of them, and only at
// times). z = 0 means "find the ground there".
struct Landmark { const char* name; float x, y, z; };
static const Landmark kLandmarks[] = {
    { "Ocean View Hotel",                219.8f, -1273.5f, 11.0f },
    { "Vercetti Estate",                -378.4f,  -536.9f, 17.2f },
    { "Malibu Club",                     499.8f,   -63.8f, 10.5f },
    { "Washington Beach building site",  148.0f,  -340.0f,  0.0f },
    { "Leaf Links Golf Club",            148.0f,   522.0f,  0.0f },
    { "InterGlobal Films studio",          8.5f,   968.8f,  9.9f },
    { "North Point Mall",                364.8f,  1051.5f, 21.0f },
    { "Viceport boatyard",              -640.9f, -1491.8f, 12.8f },
    { "Sunshine Autos",                -1014.5f,  -833.3f, 12.0f },
    { "Cherry Popper ice cream",        -863.9f,  -576.5f, 10.1f },
    { "Print works",                   -1089.0f,  -238.0f,  0.0f },
    { "Kaufman Cabs",                   -997.1f,   189.8f, 10.4f },
    { "Junkyard",                      -1300.0f,    90.0f,  0.0f },
    { "Escobar International terminal",-1446.0f,  -820.0f,  0.0f },
    { "Fort Baxter Air Base",          -1677.0f,  -178.0f,  0.0f },
};

static float Dist2D(const float* a, float x, float y) {
    return sqrtf((x - a[0]) * (x - a[0]) + (y - a[1]) * (y - a[1]));
}
static std::string DistText(float d) {
    char b[32];
    if (d >= 1000.0f) snprintf(b, sizeof b, "%.1f km", d / 1000.0f);
    else snprintf(b, sizeof b, "%.0f m", d);
    return b;
}

// Coloured dot for blips without an icon.
static void BlipDot(int type) {
    ImU32 c = type == 1 ? IM_COL32(80, 160, 255, 255)     // vehicle: blue
            : type == 2 ? IM_COL32(255, 80, 80, 255)      // person: red
            : type == 3 ? IM_COL32(120, 230, 120, 255)    // object: green
            : type == 5 ? IM_COL32(255, 220, 60, 255)     // race / event point: yellow
            :             IM_COL32(255, 120, 200, 255);   // place: pink
    ImVec2 p = ImGui::GetCursorScreenPos();
    float sz = ImGui::GetFrameHeight();
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + sz * 0.5f, p.y + sz * 0.5f), sz * 0.3f, c);
    ImGui::Dummy(ImVec2(sz, sz));
}

// "- District  (distance)" after a row.
static void WhereText(const float* me, float x, float y) {
    const char* where = DistrictAt(x, y);
    if (*where) { ImGui::SameLine(); ImGui::TextDisabled("- %s", where); }
    if (me) { ImGui::SameLine(); ImGui::TextDisabled("(%s)", DistText(Dist2D(me, x, y)).c_str()); }
}

using BlipList = std::vector<Blip>;

// One list of radar blips, nearest first, under an optional heading.
static void BlipSection(BlipList& list, const char* heading, const char* empty, const float* me,
                        const char* (*name)(const Blip&, const char*), const char* fixed = nullptr) {
    if (heading) ImGui::SeparatorText(heading);
    if (list.empty()) { if (*empty) ImGui::TextDisabled("%s", empty); return; }
    if (me)
        std::stable_sort(list.begin(), list.end(), [me](const Blip& a, const Blip& b) {
            return Dist2D(me, a.pos[0], a.pos[1]) < Dist2D(me, b.pos[0], b.pos[1]); });
    float sz = ImGui::GetFrameHeight();
    for (auto& b : list) {
        ImGui::PushID(b.index);
        if (ImGui::Button("Go")) { Blip c = b; Queue([c] { TeleportToBlip(c); }); }
        ImGui::SameLine();
        if (ImTextureID t = RadarIcon(b.sprite)) ImGui::Image(t, ImVec2(sz, sz));
        else BlipDot(b.type);
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", name(b, fixed));
        WhereText(me, b.pos[0], b.pos[1]);
        ImGui::PopID();
    }
}
static const char* SpriteName(const Blip& b, const char*) { return SpriteKind(b.sprite).name; }
static const char* FixedName(const Blip&, const char* n) { return n; }

enum { IC_PACKAGE, IC_RAMPAGE, IC_MONEY, IC_BRIBE, IC_HEALTH, IC_ARMOUR, IC_WEAPON, IC_INFO, IC_LANDMARK, IC_SAVED };
static void PlaceIcon(int kind);

static void SavedLocations(const float* pos, bool have) {
    static char name[64] = "";
    ImGui::SetNextItemWidth(240);
    ImGui::InputTextWithHint("##name", "Name for this spot", name, sizeof name);
    ImGui::SameLine();
    ImGui::BeginDisabled(!have);
    if (ImGui::Button("Save current position")) {
        auto& list = SavedPositions();
        list.push_back({ name[0] ? name : "Location " + std::to_string(list.size() + 1), pos[0], pos[1], pos[2] });
        SavePositions();
        name[0] = 0;
    }
    ImGui::EndDisabled();

    auto& list = SavedPositions();
    if (list.empty()) { ImGui::TextDisabled("Nothing saved yet. Saved spots are kept in VCTrainer_positions.txt."); return; }
    static bool nearestFirst = true;
    ImGui::Checkbox("Nearest first", &nearestFirst);
    // Sorted for display only: the file order stays, so the "saved location N"
    // hotkeys keep pointing at the same spots.
    std::vector<size_t> order(list.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    auto dist = [&](const SavedPos& p) { return have ? Dist2D(pos, p.x, p.y) : 0.0f; };
    if (nearestFirst && have)
        std::stable_sort(order.begin(), order.end(), [&](size_t x, size_t y) { return dist(list[x]) < dist(list[y]); });
    for (size_t i : order) {
        ImGui::PushID((int)i);
        if (ImGui::Button("Go")) { auto p = list[i]; Queue([p] { TeleportTo(p.x, p.y, p.z, false); }); }
        ImGui::SameLine();
        if (ImGui::Button("X")) { list.erase(list.begin() + i); SavePositions(); ImGui::PopID(); break; }
        ImGui::SameLine();
        PlaceIcon(IC_SAVED);
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%d. %s", (int)i + 1, list[i].name.c_str());
        WhereText(have ? pos : nullptr, list[i].x, list[i].y);
        ImGui::PopID();
    }
}

// One collapsible category in the list; `open` = expanded at first.
static bool PlacesTab(const char* label, int count, const char* id, bool open = false) {
    return ImGui::TreeNodeEx(id, open ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s (%d)", label, count);
}
static void EndPlacesTab() { ImGui::TreePop(); }

// Small drawn icons for list rows that have no radar icon of their own.
static void PlaceIcon(int kind) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float s = ImGui::GetFrameHeight();
    ImVec2 c(p.x + s * 0.5f, p.y + s * 0.5f);
    float r = s * 0.38f;
    const ImU32 dark = IM_COL32(20, 20, 20, 255), white = IM_COL32(255, 255, 255, 255);
    switch (kind) {
    case IC_PACKAGE:                                                      // brown parcel with a strap
        dl->AddRectFilled(ImVec2(c.x - r, c.y - r * 0.8f), ImVec2(c.x + r, c.y + r * 0.8f), IM_COL32(170, 120, 60, 255), 2);
        dl->AddLine(ImVec2(c.x, c.y - r * 0.8f), ImVec2(c.x, c.y + r * 0.8f), IM_COL32(240, 210, 140, 255), 2);
        dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), IM_COL32(240, 210, 140, 255), 2);
        break;
    case IC_RAMPAGE:                                                      // red skull
        dl->AddCircleFilled(ImVec2(c.x, c.y - r * 0.15f), r * 0.85f, IM_COL32(220, 40, 40, 255));
        dl->AddRectFilled(ImVec2(c.x - r * 0.45f, c.y + r * 0.4f), ImVec2(c.x + r * 0.45f, c.y + r * 0.9f), IM_COL32(220, 40, 40, 255), 2);
        dl->AddCircleFilled(ImVec2(c.x - r * 0.33f, c.y - r * 0.15f), r * 0.22f, dark);
        dl->AddCircleFilled(ImVec2(c.x + r * 0.33f, c.y - r * 0.15f), r * 0.22f, dark);
        break;
    case IC_MONEY: {                                                      // green coin with $
        dl->AddCircleFilled(c, r, IM_COL32(40, 170, 70, 255));
        ImVec2 ts = ImGui::CalcTextSize("$");
        dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), white, "$");
        break;
    }
    case IC_BRIBE: {                                                      // police star
        ImVec2 pts[10];
        for (int i = 0; i < 10; i++) {
            float a = -1.5708f + i * 0.6283185f, rr = (i & 1) ? r * 0.45f : r;
            pts[i] = ImVec2(c.x + cosf(a) * rr, c.y + sinf(a) * rr);
        }
        dl->AddConcavePolyFilled(pts, 10, IM_COL32(70, 130, 255, 255));
        break;
    }
    case IC_HEALTH:                                                       // red cross on white
        dl->AddRectFilled(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), white, 3);
        dl->AddRectFilled(ImVec2(c.x - r * 0.25f, c.y - r * 0.75f), ImVec2(c.x + r * 0.25f, c.y + r * 0.75f), IM_COL32(220, 30, 30, 255));
        dl->AddRectFilled(ImVec2(c.x - r * 0.75f, c.y - r * 0.25f), ImVec2(c.x + r * 0.75f, c.y + r * 0.25f), IM_COL32(220, 30, 30, 255));
        break;
    case IC_ARMOUR: {                                                     // blue shield
        ImVec2 pts[5] = { ImVec2(c.x - r * 0.8f, c.y - r * 0.8f), ImVec2(c.x + r * 0.8f, c.y - r * 0.8f),
                          ImVec2(c.x + r * 0.8f, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r * 0.8f, c.y) };
        dl->AddConvexPolyFilled(pts, 5, IM_COL32(60, 120, 230, 255));
        break;
    }
    case IC_WEAPON:                                                       // grey pistol
        dl->AddRectFilled(ImVec2(c.x - r, c.y - r * 0.5f), ImVec2(c.x + r, c.y), IM_COL32(170, 170, 180, 255), 1);
        dl->AddRectFilled(ImVec2(c.x - r, c.y), ImVec2(c.x - r * 0.35f, c.y + r * 0.85f), IM_COL32(170, 170, 180, 255), 1);
        break;
    case IC_INFO: {                                                       // white "i"
        dl->AddCircleFilled(c, r, IM_COL32(90, 90, 110, 255));
        ImVec2 ts = ImGui::CalcTextSize("i");
        dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), white, "i");
        break;
    }
    case IC_LANDMARK:                                                     // orange map pin
        dl->AddCircleFilled(ImVec2(c.x, c.y - r * 0.3f), r * 0.65f, IM_COL32(255, 150, 40, 255));
        dl->AddTriangleFilled(ImVec2(c.x - r * 0.5f, c.y), ImVec2(c.x + r * 0.5f, c.y), ImVec2(c.x, c.y + r),
                              IM_COL32(255, 150, 40, 255));
        dl->AddCircleFilled(ImVec2(c.x, c.y - r * 0.3f), r * 0.25f, white);
        break;
    case IC_SAVED:                                                        // purple flag
        dl->AddLine(ImVec2(c.x - r * 0.6f, c.y - r), ImVec2(c.x - r * 0.6f, c.y + r), white, 2);
        dl->AddTriangleFilled(ImVec2(c.x - r * 0.6f, c.y - r), ImVec2(c.x + r, c.y - r * 0.45f), ImVec2(c.x - r * 0.6f, c.y + r * 0.1f),
                              IM_COL32(190, 90, 230, 255));
        break;
    }
    ImGui::Dummy(ImVec2(s, s));
}

// Pickups lying in the world - the side activities that have no radar
// marker: hidden packages, rampages, money, bribes, health/armour, weapons.
enum { PK_PACKAGE, PK_RAMPAGE, PK_MONEY, PK_BRIBE, PK_HEALTH, PK_WEAPON, PK_OTHER, PK_COUNT };
static int PickupKind(int model) {
    switch (model) {
    case 410: return PK_PACKAGE;                              // package1
    case 383: return PK_RAMPAGE;                              // killfrenzy
    case 337: case 335: return PK_MONEY;                      // money, briefcase
    case 375: return PK_BRIBE;
    case 366: case 367: case 368: return PK_HEALTH;           // health, adrenaline, body armour
    }
    if (model >= 258 && model <= 294) return PK_WEAPON;
    return PK_OTHER;
}
static const char* PickupName(int model) {
    static const char* weapons[] = {
        "Cellphone", "Brass knuckles", "Screwdriver", "Golf club", "Nightstick", "Knife", "Baseball bat",
        "Hammer", "Meat cleaver", "Machete", "Katana", "Chainsaw", "Grenades", "Tear gas", "Molotovs",
        "Rocket", "Pistol", "Python", "Ruger", "Shotgun", "SPAS-12", "Stubby shotgun", "M4", "Tec-9",
        "Uzi", "Ingram", "MP5", "Sniper rifle", "Laser sniper", "Rocket launcher", "Flamethrower", "M60",
        "Minigun", "Detonator", "Camera", "Fingers", "Silenced Ingram" };
    switch (model) {
    case 410: return "Hidden package";
    case 383: return "Rampage";
    case 337: return "Money";
    case 335: return "Briefcase";
    case 375: return "Police bribe";
    case 366: return "Health";
    case 367: return "Adrenaline";
    case 368: return "Body armour";
    case 365: return "Info";
    case 382: return "Camera";
    case 411: return "Save point";
    case 406: case 407: return "Property";
    }
    if (model >= 258 && model < 258 + (int)(sizeof weapons / sizeof *weapons)) return weapons[model - 258];
    return "Pickup";
}

static int PickupIcon(int model) {
    switch (PickupKind(model)) {
    case PK_PACKAGE: return IC_PACKAGE;
    case PK_RAMPAGE: return IC_RAMPAGE;
    case PK_MONEY:   return IC_MONEY;
    case PK_BRIBE:   return IC_BRIBE;
    case PK_HEALTH:  return model == 368 ? IC_ARMOUR : IC_HEALTH;
    case PK_WEAPON:  return IC_WEAPON;
    }
    return IC_INFO;
}

static void PickupLists(const float* me) {
    std::vector<Pickup> kinds[PK_COUNT];
    for (auto& p : ActivePickups()) kinds[PickupKind(p.model)].push_back(p);
    static const char* titles[PK_COUNT] = { "Hidden packages", "Rampages", "Money pickups", "Police bribes",
                                            "Health & armour", "Weapons", "Other pickups" };
    static const char* ids[PK_COUNT] = { "pk_pkg", "pk_ramp", "pk_money", "pk_bribe", "pk_health", "pk_weap", "pk_other" };
    for (int k = 0; k < PK_COUNT; k++) {
        auto& list = kinds[k];
        if (!PlacesTab(titles[k], (int)list.size(), ids[k])) continue;
        if (list.empty()) ImGui::TextDisabled("None left to collect.");
        if (me)
            std::stable_sort(list.begin(), list.end(), [me](const Pickup& a, const Pickup& b) {
                return Dist2D(me, a.pos[0], a.pos[1]) < Dist2D(me, b.pos[0], b.pos[1]); });
        for (auto& p : list) {
            ImGui::PushID(p.index);
            if (ImGui::Button("Go")) { Pickup c = p; Queue([c] { TeleportToPickup(c); }); }
            ImGui::SameLine();
            PlaceIcon(PickupIcon(p.model));
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", PickupName(p.model));
            WhereText(me, p.pos[0], p.pos[1]);
            ImGui::PopID();
        }
        EndPlacesTab();
    }
}

static void TeleportTab() {
    float pos[3] = {};
    bool have = GetPlayerPos(pos);
    const float* me = have ? pos : nullptr;
    Section("Coordinates");
    if (have) ImGui::Text("X %.2f   Y %.2f   Z %.2f   %s", pos[0], pos[1], pos[2], DistrictAt(pos[0], pos[1]));

    static float target[3] = { 0, 0, 20 };
    static bool ground = true;
    ImGui::SetNextItemWidth(300);
    ImGui::InputFloat3("##xyz", target, "%.1f");
    ImGui::SameLine();
    if (ImGui::Button("Teleport")) {
        float x = target[0], y = target[1], z = target[2]; bool g = ground;
        Queue([x, y, z, g] { TeleportTo(x, y, z, g); });
    }
    ImGui::Checkbox("Snap to ground", &ground);
    ImGui::SameLine();
    if (have && ImGui::Button("Use my position")) memcpy(target, pos, sizeof target);
    ImGui::SameLine();
    if (ImGui::Button("Teleport to mission marker")) Queue([] { TeleportToMarker(); });

    // Sort out what is on the radar right now. Blips without an icon belong
    // to the running mission, except yellow contact points (type 5), which
    // are race and side-activity starts.
    enum { M_PERSON, M_VEHICLE, M_OBJECT, M_PLACE, M_COUNT };
    BlipList mission[M_COUNT], cats[PC_COUNT], homes, forSale;
    for (auto& b : ActiveBlips()) {
        if (b.sprite >= 1 && b.sprite <= 3) continue;          // player arrow, north, etc.
        if (b.sprite == 0) {
            if (b.type == 5) cats[PC_EVENTS].push_back(b);
            else mission[b.type == 2 ? M_PERSON : b.type == 1 ? M_VEHICLE : b.type == 3 ? M_OBJECT : M_PLACE].push_back(b);
        } else if (b.sprite == 19) homes.push_back(b);
        else if (b.sprite == 25) forSale.push_back(b);
        else cats[SpriteKind(b.sprite).cat].push_back(b);
    }
    int nMission = 0;
    for (auto& l : mission) nMission += (int)l.size();

    Section("Places");
    if (PlacesTab("Current mission", nMission, "mission", true)) {
        if (!nMission) ImGui::TextDisabled("The current mission isn't marking anything right now.");
        static const char* heads[M_COUNT] = { "People", "Vehicles", "Objects", "Destinations" };
        static const char* names[M_COUNT] = { "Person", "Vehicle", "Object", "Destination" };
        for (int i = 0; i < M_COUNT; i++)
            if (!mission[i].empty()) BlipSection(mission[i], heads[i], "", me, FixedName, names[i]);
        EndPlacesTab();
    }
    if (PlacesTab("Races & events", (int)cats[PC_EVENTS].size(), "events")) {
        ImGui::TextDisabled("Yellow contact points: race starts and other side activities, not story missions.");
        BlipSection(cats[PC_EVENTS], nullptr, "None on the radar right now.", me, FixedName, "Race / event start");
        EndPlacesTab();
    }
    if (PlacesTab("Mission contacts", (int)cats[PC_CONTACTS].size(), "contacts")) {
        ImGui::TextDisabled("Where story missions start: the people who give them, and payphones.");
        BlipSection(cats[PC_CONTACTS], nullptr, "No mission contacts on the radar right now.", me, SpriteName);
        EndPlacesTab();
    }
    if (PlacesTab("Safehouses", (int)(homes.size() + forSale.size()), "homes")) {
        BlipSection(homes, "Your safehouses", "None owned yet.", me, SpriteName);
        BlipSection(forSale, "Property for sale", "Nothing for sale on the radar.", me, SpriteName);
        EndPlacesTab();
    }
    if (PlacesTab("Businesses", (int)cats[PC_BUSINESS].size(), "business")) {
        ImGui::TextDisabled("Asset properties: ones for sale and ones you own.");
        BlipSection(cats[PC_BUSINESS], nullptr, "None on the radar right now.", me, SpriteName);
        EndPlacesTab();
    }
    if (PlacesTab("Shops", (int)cats[PC_SHOPS].size(), "shops")) {
        BlipSection(cats[PC_SHOPS], nullptr, "None on the radar right now.", me, SpriteName);
        if (!cats[PC_OTHER].empty()) BlipSection(cats[PC_OTHER], "Other map icons", "", me, SpriteName);
        EndPlacesTab();
    }
    PickupLists(me);
    const int nLand = (int)(sizeof kLandmarks / sizeof *kLandmarks);
    if (PlacesTab("Landmarks", nLand, "landmarks")) {
        std::vector<const Landmark*> lm;
        for (auto& l : kLandmarks) lm.push_back(&l);
        if (have)
            std::stable_sort(lm.begin(), lm.end(), [&](const Landmark* a, const Landmark* b) {
                return Dist2D(pos, a->x, a->y) < Dist2D(pos, b->x, b->y); });
        for (auto* l : lm) {
            ImGui::PushID(l->name);
            if (ImGui::Button("Go")) {
                Landmark c = *l;
                Queue([c] { TeleportTo(c.x, c.y, c.z != 0.0f ? c.z + 1.0f : 0.0f, c.z == 0.0f); });
            }
            ImGui::SameLine();
            PlaceIcon(IC_LANDMARK);
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", l->name);
            WhereText(me, l->x, l->y);
            ImGui::PopID();
        }
        EndPlacesTab();
    }

    Section("Saved locations");
    SavedLocations(pos, have);
}

static void CheatsTab() {
    ImGui::TextDisabled("Checkboxes are cheats that stay on: tick to turn on, untick to turn off.\n"
                        "Buttons happen once. Hover anything to see the original code.");
    bool inGame = PlayerPed() != nullptr;
    ImGui::BeginChild("cheats", ImVec2(0, 0), ImGuiChildFlags_None);
    for (auto& g : kCheats) {
        Section(g.group);
        if (ImGui::BeginTable(g.group, 3, ImGuiTableFlags_SizingStretchSame)) {
            for (auto& c : g.c) {
                if (!c.code) break;
                ImGui::TableNextColumn();
                const char* code = c.code;
                ImGui::PushID(code);
                if (NativeCheatHasState(code)) {
                    bool on = inGame && NativeCheatOn(code);
                    const bool green = on;
                    if (green) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.95f, 0.65f, 1.0f));
                    if (ImGui::Checkbox(c.desc, &on)) Queue([code, on] { SetNativeCheat(code, on); });
                    if (green) ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\nStays on until you untick it.", code);
                } else {
                    bool oneWay = NativeCheatIsOneWay(code);
                    if (ImGui::Button(c.desc, ImVec2(-FLT_MIN, 0))) Queue([code] { RunNativeCheat(code); });
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip(oneWay ? "%s\nCan't be switched off - reload your save to undo it."
                                                 : "%s", code);
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}

static void ActiveTab() {
    auto items = ActiveCheats();
    if (!S.enabled) {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.4f, 1.0f), "The trainer is OFF - nothing from it is running.");
        return;
    }
    if (items.empty()) {
        ImGui::TextDisabled("Nothing is switched on right now.");
        return;
    }
    ImGui::Text("%d active", (int)items.size());
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 170);
    if (ImGui::Button("Turn everything off", ImVec2(170, 0))) Queue([] { TurnEverythingOff(); });

    ImGui::BeginChild("active", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (ImGui::BeginTable("activelist", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Cheat", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Where", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("Off", ImGuiTableColumnFlags_WidthFixed, 70);
        for (size_t i = 0; i < items.size(); i++) {
            ImGui::PushID((int)i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.40f, 0.95f, 0.65f, 1.0f), "%s", items[i].label.c_str());
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", items[i].detail.c_str());
            ImGui::TableNextColumn();
            auto off = items[i].turnOff;
            if (ImGui::Button("Turn off", ImVec2(-FLT_MIN, 0))) Queue(off);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

static void HotkeysTab() {
    auto& acts = HotkeyActions();
    ImGui::TextDisabled("Click a key box, then press a key or combo (Ctrl/Shift/Alt + key). "
                        "Esc clears. INSERT/F7 open the menu.");

    static char filter[48] = "";
    static bool onlyBound = false;
    ImGui::SetNextItemWidth(220);
    ImGui::InputTextWithHint("##hkfilter", "Search features...", filter, sizeof filter);
    ImGui::SameLine();
    ImGui::Checkbox("Only bound", &onlyBound);
    ImGui::SameLine();
    int bound = 0;
    for (auto& a : acts) if (a.vk) bound++;
    ImGui::TextDisabled("%d bound", bound);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 80);
    if (ImGui::Button("Clear all", ImVec2(80, 0))) ImGui::OpenPopup("clearall");
    if (ImGui::BeginPopup("clearall")) {
        ImGui::Text("Remove every hotkey?");
        if (ImGui::Button("Yes, clear all")) {
            for (auto& a : acts) { a.vk = 0; a.mods = 0; }
            SaveSettings();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    std::string f = filter;
    for (auto& ch : f) ch = (char)tolower(ch);
    auto visible = [&](const HotkeyAction& a) {
        if (onlyBound && !a.vk) return false;
        if (f.empty()) return true;
        std::string l = a.label + " " + a.category;
        for (auto& ch : l) ch = (char)tolower(ch);
        return l.find(f) != std::string::npos;
    };
    // Which other feature uses the same key, for a warning next to clashes.
    auto clashWith = [&](size_t i) -> const HotkeyAction* {
        for (size_t j = 0; j < acts.size(); j++)
            if (j != i && acts[j].vk && acts[j].vk == acts[i].vk && acts[j].mods == acts[i].mods) return &acts[j];
        return nullptr;
    };

    ImGui::BeginChild("hklist", ImVec2(0, 0), ImGuiChildFlags_Borders);
    std::vector<std::string> cats;
    for (auto& a : acts)
        if (std::find(cats.begin(), cats.end(), a.category) == cats.end()) cats.push_back(a.category);

    for (auto& cat : cats) {
        int shown = 0;
        for (auto& a : acts) if (a.category == cat && visible(a)) shown++;
        if (!shown) continue;
        bool big = cat == "Give weapon" || cat == "Game cheats";
        if (!f.empty() || onlyBound) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        else ImGui::SetNextItemOpen(!big, ImGuiCond_Once);
        if (!ImGui::CollapsingHeader((cat + "##hk").c_str())) continue;

        if (ImGui::BeginTable(cat.c_str(), 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
                                              ImGuiTableFlags_PadOuterX)) {
            ImGui::TableSetupColumn("Feature", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 190);
            ImGui::TableSetupColumn("Clear", ImGuiTableColumnFlags_WidthFixed, 24);
            for (size_t i = 0; i < acts.size(); i++) {
                HotkeyAction& a = acts[i];
                if (a.category != cat || !visible(a)) continue;
                ImGui::PushID((int)i);
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(a.label.c_str());
                if (a.kind != HotkeyAction::Press) {
                    ImGui::SameLine();
                    ImGui::TextDisabled(a.kind == HotkeyAction::Toggle ? "on/off" : "hold");
                }
                if (const HotkeyAction* other = clashWith(i)) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "!");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Same key as \"%s\"", other->label.c_str());
                }

                ImGui::TableNextColumn();
                char name[80];
                bool capturing = g_capturingHotkey == (int)i;
                if (capturing) {
                    int mods = CurrentModifiers();
                    snprintf(name, sizeof name, "%s%s%s...", (mods & HKMOD_CTRL) ? "Ctrl + " : "",
                             (mods & HKMOD_SHIFT) ? "Shift + " : "", (mods & HKMOD_ALT) ? "Alt + " : "Press keys");
                } else {
                    HotkeyName(a.vk, a.mods, name, sizeof name);
                }
                if (capturing) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
                else if (!a.vk) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
                if (ImGui::Button(name, ImVec2(-FLT_MIN, 0))) g_capturingHotkey = capturing ? -1 : (int)i;
                if (capturing || !a.vk) ImGui::PopStyleColor();

                ImGui::TableNextColumn();
                ImGui::BeginDisabled(!a.vk);
                if (ImGui::Button("x", ImVec2(24, 0))) { a.vk = 0; a.mods = 0; SaveSettings(); }
                ImGui::EndDisabled();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}

static void SettingsTab() {
    Section("Controls");
    ImGui::BulletText("INSERT or F7 opens and closes this menu.");
    ImGui::BulletText("While it is open the game ignores your mouse and keyboard.");
    ImGui::BulletText("Tab / arrow keys also navigate the menu.");
    ImGui::SetNextItemWidth(200);
    ImGui::SliderFloat("Mouse speed", &g_mouseSensitivity, 0.25f, 4.0f, "%.2f");
    Section("On-screen overlay (shown while playing)");
    ImGui::Checkbox("Coordinates", &S.overlayCoords); ImGui::SameLine(160);
    ImGui::Checkbox("Vehicle speed", &S.overlaySpeed); ImGui::SameLine(320);
    ImGui::Checkbox("FPS", &S.overlayFps);
    ImGui::Checkbox("Active cheats list", &S.overlayActive);
    Section("Settings file");
    ImGui::TextWrapped("Toggles, hotkeys and mouse speed are saved to VCTrainer.ini when you close the menu.");
    if (ImGui::Button("Save now")) SaveSettings();
    Section("About");
    ImGui::TextWrapped("Modern VC Trainer for GTA Vice City v1.0, built with Dear ImGui %s.", IMGUI_VERSION);
}

static void DrawOverlay() {
    if (!OverlayEnabled()) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Size.x - 10, 10), ImGuiCond_Always, ImVec2(1, 0));
    ImGui::SetNextWindowBgAlpha(0.45f);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("##overlay", nullptr, f)) {
        float p[3];
        if (S.overlayCoords && GetPlayerPos(p)) ImGui::Text("%.1f, %.1f, %.1f", p[0], p[1], p[2]);
        if (S.overlaySpeed && PlayerVehicle()) ImGui::Text("%.0f km/h", VehicleSpeedKmh());
        if (S.overlayFps) ImGui::Text("%.0f FPS", ImGui::GetIO().Framerate);
        if (S.overlayActive) {
            auto items = ActiveCheats();
            if (S.overlayCoords || S.overlaySpeed || S.overlayFps) ImGui::Separator();
            if (items.empty()) ImGui::TextDisabled("No cheats active");
            for (auto& a : items) ImGui::TextColored(ImVec4(0.40f, 0.95f, 0.65f, 1.0f), "%s", a.label.c_str());
        }
    }
    ImGui::End();
}

// Enemy markers: a box round every hostile ped - solid red when you can see
// them, dashed orange when they're behind a wall - with the distance, a
// crosshair on the one the aim lock is on, and arrows at the screen edge for
// the ones behind you or off to the side.
static void DrawEnemyMarks() {
    auto marks = EnemyMarks();
    if (marks.empty()) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    float sx = *game::ScreenWidth > 0 ? ds.x / *game::ScreenWidth : 1.0f;
    float sy = *game::ScreenHeight > 0 ? ds.y / *game::ScreenHeight : 1.0f;
    const ImU32 seen = IM_COL32(255, 50, 50, 235), hidden = IM_COL32(255, 170, 30, 220);
    const ImU32 lockCol = IM_COL32(255, 255, 255, 255), shadow = IM_COL32(0, 0, 0, 170);
    for (auto& m : marks) {
        ImU32 col = m.visible ? seen : hidden;
        char txt[24]; snprintf(txt, sizeof txt, "%.0f m", m.dist);
        if (m.onScreen) {
            float top = m.top * sy, bot = m.bottom * sy, x = m.x * sx;
            float h = fmaxf(bot - top, 8.0f), w = h * 0.45f;
            ImVec2 a(x - w * 0.5f, top), b(x + w * 0.5f, top + h);
            dl->AddRect(ImVec2(a.x - 1, a.y - 1), ImVec2(b.x + 1, b.y + 1), shadow, 0, 0, 3.0f);
            if (m.visible) dl->AddRect(a, b, col, 0, 0, m.locked ? 3.0f : 2.0f);
            else {                                                     // dashed: behind something
                float seg = fmaxf(h / 8.0f, 3.0f);
                auto dash = [&](ImVec2 p0, ImVec2 p1) {
                    float len = sqrtf((p1.x - p0.x) * (p1.x - p0.x) + (p1.y - p0.y) * (p1.y - p0.y));
                    for (float t = 0; t < len; t += seg * 2) {
                        float t1 = fminf(t + seg, len);
                        dl->AddLine(ImVec2(p0.x + (p1.x - p0.x) * t / len, p0.y + (p1.y - p0.y) * t / len),
                                    ImVec2(p0.x + (p1.x - p0.x) * t1 / len, p0.y + (p1.y - p0.y) * t1 / len), col, 2.0f);
                    }
                };
                dash(a, ImVec2(b.x, a.y)); dash(ImVec2(b.x, a.y), b); dash(b, ImVec2(a.x, b.y)); dash(ImVec2(a.x, b.y), a);
            }
            dl->AddTriangleFilled(ImVec2(x - 6, top - 12), ImVec2(x + 6, top - 12), ImVec2(x, top - 3), col);
            ImVec2 ts = ImGui::CalcTextSize(txt);
            dl->AddText(ImVec2(x - ts.x * 0.5f + 1, b.y + 3), shadow, txt);
            dl->AddText(ImVec2(x - ts.x * 0.5f, b.y + 2), col, txt);
            if (m.locked) {
                ImVec2 c(x, top + h * 0.3f);
                dl->AddCircle(c, w * 0.6f + 4, lockCol, 24, 2.0f);
                dl->AddLine(ImVec2(c.x - w - 8, c.y), ImVec2(c.x - w * 0.3f, c.y), lockCol, 2.0f);
                dl->AddLine(ImVec2(c.x + w * 0.3f, c.y), ImVec2(c.x + w + 8, c.y), lockCol, 2.0f);
            }
        } else {
            // arrow on a ring round the screen centre, pointing at them
            float ang = atan2f(m.dirX, m.dirY);                          // 0 = ahead, + = right
            ImVec2 c(ds.x * 0.5f, ds.y * 0.5f);
            float r = fminf(ds.x, ds.y) * 0.42f;
            float s = sinf(ang), co = cosf(ang);
            ImVec2 tip(c.x + s * r, c.y - co * r);
            ImVec2 base(c.x + s * (r - 22), c.y - co * (r - 22));
            ImVec2 side(co * 10, s * 10);
            dl->AddTriangleFilled(tip, ImVec2(base.x + side.x, base.y + side.y), ImVec2(base.x - side.x, base.y - side.y), col);
            dl->AddTriangle(tip, ImVec2(base.x + side.x, base.y + side.y), ImVec2(base.x - side.x, base.y - side.y), shadow, 1.5f);
            ImVec2 ts = ImGui::CalcTextSize(txt);
            ImVec2 tp(base.x - s * 14 - ts.x * 0.5f, base.y + co * 14 - ts.y * 0.5f);
            dl->AddText(ImVec2(tp.x + 1, tp.y + 1), shadow, txt);
            dl->AddText(tp, col, txt);
        }
    }
}

void DrawTrainerMenu() {
    DrawEnemyMarks();
    DrawOverlay();
    if (!g_menuOpen) return;
    ImGui::SetNextWindowSize(ImVec2(600, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Modern VC Trainer###VC Trainer", &g_menuOpen, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }

    // Master switch: pauses every cheat at once without forgetting what was on.
    {
        bool on = S.enabled;
        ImVec4 col = on ? ImVec4(0.10f, 0.62f, 0.38f, 1.0f) : ImVec4(0.55f, 0.12f, 0.16f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x + 0.1f, col.y + 0.1f, col.z + 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
        if (ImGui::Button(on ? "TRAINER ON" : "TRAINER OFF", ImVec2(140, 0))) Queue([on] { SetTrainerEnabled(!on); });
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        ImGui::TextDisabled(on ? "Click to pause every cheat at once."
                               : "All cheats paused (your choices are kept). Only its hotkey works.");
    }

    if (ImGui::BeginTabBar("tabs")) {
        char activeLabel[32];
        snprintf(activeLabel, sizeof activeLabel, "Active (%d)###active", (int)ActiveCheats().size());
        if (ImGui::BeginTabItem(activeLabel)) { ActiveTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Player"))   { PlayerTab();   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Weapons"))  { WeaponsTab();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Vehicle"))  { VehicleTab();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("World"))    { WorldTab();    ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Teleport")) { TeleportTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Cheats"))   { CheatsTab();   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Hotkeys"))  { HotkeysTab();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Settings")) { SettingsTab(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
