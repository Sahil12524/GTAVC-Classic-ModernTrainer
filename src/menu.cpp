// The ImGui menu. Everything that touches game objects is pushed onto the
// action queue (Queue) and runs at the start of the next game frame.
#include "features.h"
#include "trainer.h"
#include "game.h"
#include "imgui.h"
#include "tables.h"
#include <algorithm>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
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
    ImGui::Checkbox("Infinite sprint", &S.neverTired);   ImGui::SameLine(200);
    ImGui::Checkbox("Fast reload", &S.fastReload);

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
    if (ImGui::Button("Unlock doors")) Queue([] { SetDoorLock(false); });
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

    Section("Missions");
    ImGui::Checkbox("Disable mission timers (countdown never runs out)", &S.freezeMissionTimer);
}

static void TeleportTab() {
    float pos[3] = {};
    bool have = GetPlayerPos(pos);
    Section("Position");
    if (have) ImGui::Text("X %.2f   Y %.2f   Z %.2f", pos[0], pos[1], pos[2]);

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

    Section("Radar");
    if (ImGui::Button("Teleport to mission marker")) Queue([] { TeleportToMarker(); });
    auto blips = ActiveBlips();
    if (ImGui::TreeNode("blips", "All radar blips (%d)", (int)blips.size())) {
        const char* kinds[] = { "", "Vehicle", "Person", "", "Marker", "Contact" };
        for (auto& b : blips) {
            ImGui::PushID(b.index);
            if (ImGui::SmallButton("Go")) { Blip c = b; Queue([c] { TeleportToBlip(c); }); }
            ImGui::SameLine();
            float d = have ? sqrtf((b.pos[0] - pos[0]) * (b.pos[0] - pos[0]) + (b.pos[1] - pos[1]) * (b.pos[1] - pos[1])) : 0;
            if (b.sprite) ImGui::Text("Map icon %d", b.sprite);
            else ImGui::Text("%s", kinds[b.type]);
            ImGui::SameLine(); ImGui::TextDisabled("%.0fm away", d);
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    Section("Saved locations");
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
    if (list.empty()) ImGui::TextDisabled("Nothing saved yet. Saved spots are kept in VCTrainer_positions.txt.");
    ImGui::BeginChild("saved", ImVec2(0, 0), ImGuiChildFlags_None);
    for (size_t i = 0; i < list.size(); i++) {
        ImGui::PushID((int)i);
        if (ImGui::Button("Go")) { auto p = list[i]; Queue([p] { TeleportTo(p.x, p.y, p.z, false); }); }
        ImGui::SameLine();
        if (ImGui::Button("X")) { list.erase(list.begin() + i); SavePositions(); ImGui::PopID(); break; }
        ImGui::SameLine();
        ImGui::Text("%s", list[i].name.c_str());
        ImGui::SameLine(); ImGui::TextDisabled("(%.0f, %.0f, %.0f)", list[i].x, list[i].y, list[i].z);
        ImGui::PopID();
    }
    ImGui::EndChild();
}

static void CheatsTab() {
    ImGui::TextDisabled("Checkboxes are cheats that stay on: tick to turn on, untick to turn off.\n"
                        "Buttons happen once. Hover anything to see the original code.");
    bool inGame = PlayerPed() != nullptr;
    ImGui::BeginChild("cheats", ImVec2(0, 0), ImGuiChildFlags_None);
    for (auto& g : kCheats) {
        Section(g.group);
        int n = 0;
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
                n++;
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
    ImGui::TextWrapped("VC Trainer for GTA Vice City v1.0, built with Dear ImGui %s.", IMGUI_VERSION);
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

void DrawTrainerMenu() {
    DrawOverlay();
    if (!g_menuOpen) return;
    ImGui::SetNextWindowSize(ImVec2(600, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("VC Trainer", &g_menuOpen, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }

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
