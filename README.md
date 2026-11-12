# Modern VC Trainer

An in-game trainer for the classic **GTA: Vice City (PC, v1.0)** with a modern
[Dear ImGui](https://github.com/ocornut/imgui) menu drawn inside the game.
No separate program to run: it's an `.asi` plugin for Ultimate ASI Loader - drop
`VCTrainer.asi` in the game folder, start the game, press **INSERT** (or **F7**).
Works side by side with CLEO and other mods.

> Fan-made and unofficial. Not affiliated with or endorsed by Rockstar Games or
> Take-Two Interactive. This repository contains no game files. Single-player only.

---

## Contents

- [Features](#features)
- [Requirements](#requirements)
- [Installing](#installing)
- [Using the trainer](#using-the-trainer)
  - [Menu controls](#menu-controls)
  - [Tabs](#tabs)
  - [Hotkeys](#hotkeys)
  - [Fly mode](#fly-mode)
  - [Movement: super jump, super run, bunny hop and surf](#movement-super-jump-super-run-bunny-hop-and-surf)
  - [Combat: aim lock, enemy markers and weapon options](#combat-aim-lock-enemy-markers-and-weapon-options)
  - [Master switch](#master-switch)
  - [Files the trainer writes](#files-the-trainer-writes)
- [Uninstalling](#uninstalling)
- [Troubleshooting](#troubleshooting)
- [How it works](#how-it-works)
- [Building from source](#building-from-source)
- [Project layout](#project-layout)
- [Credits and licence](#credits-and-licence)

---

## Features

**Player**
- Infinite health, infinite armour, never wanted, infinite ammo, no reload, infinite sprint, fast reload
- Bulletproof, fireproof, explosion-proof, collision-proof, melee-proof
- Health/armour sliders, max health and max armour up to 250 (remembered between sessions)
- Wanted level 0–6 stars, **lock the wanted level at a maximum** you choose, money (+$1K … +$1M, set any amount, take money)
- Suicide that always works, even with god mode or in a vehicle
- **Never wanted** also stops busts: cops can't arrest you or pull you out of a car for it
- **No ragdoll:** cars, explosions, punches and falls never knock you down
- **Fly mode / no clip:** fly anywhere through walls and into the sky, on foot or in a vehicle; shoot, throw and switch weapons while flying
- **Movement:** super jump, super run, CS:GO-style **auto bunny hop** (AFH / ABH, air strafing) and **ramp surfing**, no landing roll

**Combat**
- **Aim lock-on:** while aiming, the crosshair snaps onto the nearest enemy anywhere around you (gang members, police, criminals, anyone attacking you, police helicopters), leads moving targets, follows knocked-down enemies; silent aim while bunny hopping
- **Show enemies through walls:** boxes round every enemy (solid when visible, dashed behind walls), distance, and edge arrows for ones off screen
- Riot and "peds attack you" game cheats make every ped a target

**Weapons**
- Give any of the 33 weapons with the ammo you choose; the game's three weapon sets
- **Rapid fire** for every gun (hold the button, about 22 shots a second)
- **Wall bang:** bullets go through walls, cars and wrecks
- **One-shot vehicles:** any vehicle you shoot blows up, tanks and police helicopters included
- **No-scope sniper / rocket launcher:** fire from the hip with one click, along the crosshair or at the locked enemy
- **Sniper full auto:** hold the button to keep firing, in the scope or from the hip
- Infinite ammo leaves the remote grenade detonator alone, so the detonator never gets stuck in your hand

**Vehicles**
- Spawn any car, bike, boat, helicopter or RC vehicle (searchable list with in-game names), optionally seated in it
- Repair, flip upright, lock/unlock doors, boost, jump, stop, repaint from the game's 95-colour palette, get into the nearest vehicle
- **Auto lock doors** when you get in (cars with doors), unlocked again when you get out
- **Flip upright keeps your speed**; **Auto flip** (cars only) turns your car back over the moment it lands on its roof
- Car god mode (tank shells and rockets included), auto repair, bulletproof / fireproof / explosion-proof / solid / melee-proof car
- **Survive vehicle explosions:** stay inside the wreck, or be thrown clear beside it
- **Never fall off bikes:** crashes, hits and shots can't throw you off, but peds can still pull you off and pressing exit still gets you off
- **Heavy car (like the Rhino):** your car gets tank weight, and any vehicle it touches is **pushed far away** or **blown up**
- **Kill peds on one touch** with any vehicle you drive

**World**
- Set or freeze the time, force any weather, game speed (slow motion / fast)
- Pedestrian and traffic density
- Disable mission countdown timers, including rampage timers
- **Genocide mode:** every ped dies and every vehicle around you blows up, new ones too (you, your vehicle and passengers are spared)

**Teleport**
- Categorised place list, each nearest first with district and distance: current mission (people, vehicles, objects, destinations), races and events, mission contacts, safehouses, businesses, shops, hidden packages, rampages, money, bribes, health and armour, weapons, other pickups, landmarks, saved locations
- Mission-marker hotkey that never loops on a marker you're already at; in an ambulance it picks up patients until full, then goes to the drop-off
- Teleports use the game's own method (collision and scene loaded first) and land on a clear spot: no walls, other vehicles, people or water, and vehicles never go under roofs or onto rooftops
- Moving targets: you arrive right behind them, matching their speed

**Game cheats**
- All 58 built-in Vice City cheats, one click each (decoded straight from the game)
- Cheats that stay on get a real **on/off switch** showing their true state, including ones the game itself can't turn off
- **Active** tab and optional on-screen list of everything currently switched on, with one-click turn off

**Quality of life**
- Every feature can be put on a hotkey, including key combinations like `Ctrl + G`, `Shift + F`, `Alt + 1` and mouse buttons
- Master ON/OFF switch that pauses every cheat at once
- Optional on-screen overlay: coordinates, vehicle speed, FPS
- Settings and hotkeys are remembered between sessions

---

## Requirements

- **GTA: Vice City for PC, version 1.0** (the `gta-vc.exe` that is **3,088,896 bytes**).
  Other versions (1.1, Steam, Rockstar Launcher, Definitive Edition) use different
  memory addresses. On those the trainer shows a warning at startup and stays switched off;
  it will not touch the game.
- Windows 10 or 11 (also expected to work on 7/8 with the Universal C Runtime installed).

How to check your version: right-click `gta-vc.exe` → **Properties** → the size should
read **3,088,896 bytes**.

---

## Installing

The trainer is an `.asi` plugin, loaded by Ultimate ASI Loader - the standard way to add
mods to Vice City, so it works alongside CLEO, SilentPatch, widescreen fixes and other `.asi` mods.

1. Install [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest):
   download `Ultimate-ASI-Loader.zip` (the 32-bit one, **not** `_x64`) and put its
   `dinput8.dll` next to `gta-vc.exe`. Keep its name as it is. (Already have it for other mods?
   Skip this step.)
2. Download `VCTrainer.asi` from the [latest release](../../releases/latest) and put it next to
   `gta-vc.exe` (or in a `scripts` folder inside the game folder: the loader looks in both).
3. Start the game normally and load a save (or start a new game).
4. A message appears in the top-left help box: *"Modern VC Trainer loaded. Press INSERT or F7 to open the menu."*
5. Press **INSERT** or **F7**.

Any other mods are installed the usual way, for example CLEO's files or more `.asi` files in the
game folder or `scripts`. The trainer doesn't need to be renamed or changed for them. If the
trainer is somehow installed twice, only the first copy runs.

> **Upgrading from an older release that shipped `dinput8.dll`:** delete that old trainer
> `dinput8.dll`, install Ultimate ASI Loader as above and add `VCTrainer.asi`. Your settings,
> hotkeys and saved locations are kept, because they live in their own files.

### Playing nicely with other mods

The trainer's hooks chain to whatever is already there. If another mod (CLEO, SilentPatch,
a graphics mod's `d3d8.dll`, ...) has hooked the same place in the game first, the trainer
calls through to it instead of refusing to load or switching it off. Mods that hook after
the trainer usually chain through it the same way.

---

## Using the trainer

### Menu controls

| Action | Key |
|---|---|
| Open / close the menu | **INSERT** or **F7** |
| Move the cursor, click | Mouse |
| Navigate without the mouse | Tab / arrow keys / Enter |
| Close | INSERT, F7, or the **×** in the title bar |

While the menu is open the game ignores your mouse and keyboard, and the camera is
swung straight behind your character and held still, so clicking never shoots or turns
the camera. Close the menu to play again.

The menu only appears **during gameplay**: it is not available on the game's main menu
or pause screen.

### Tabs

| Tab | What's in it |
|---|---|
| **Player** | Health/armour cheats, never wanted, sprint, reload, aim lock-on, no ragdoll, show enemies through walls, proofs, movement (super jump, super run, bunny hop, surf, no landing roll), fly mode, health & armour sliders, max health/armour, wanted stars and wanted lock, money |
| **Weapons** | Infinite ammo / no reload, wall bang, one-shot vehicles, rapid fire, no-scope sniper, sniper full auto, the three weapon sets, a button for every weapon, custom ammo amount |
| **Vehicle** | Current-vehicle tools (repair, flip, doors, auto lock, boost, jump, stop), car god mode and protection, survive explosions, never fall off bike, heavy car (just heavy / push / blow up), kill peds on one touch, auto flip, paint, and the vehicle spawner |
| **World** | Time, freeze time, weather, game speed, pedestrian and traffic density, genocide mode, mission and rampage timers |
| **Teleport** | Your coordinates and district, teleport to coordinates, mission marker, and the categorised place list (see Features), plus saved locations |
| **Active (N)** | Everything switched on right now, from the trainer and from game cheats, with a **Turn off** button on each and **Turn everything off** |
| **Cheats** | All 58 built-in game cheats, grouped (player, weapons, weather, vehicle spawns, vehicles, pedestrians, skins). Cheats that stay on are **checkboxes**: tick to turn on, untick to turn off. One-off cheats are buttons. Hover anything to see the original code |
| **Hotkeys** | Bind any feature to a key (see below) |
| **Settings** | Mouse speed for the menu cursor, on-screen overlay, save settings |

Tips:
- **Turning game cheats off:** in the original game, typing a cheat again sometimes switches it off, but it always says "Cheat activated", and some cheats (green lights, aggressive/pink/black traffic, women follow you, media meter, fat/skinny) can never be switched off at all. The trainer reads each cheat's real switch inside the game, so the checkboxes always show the true state and unticking really turns it off. The only exceptions are *Peds riot* and *Peds attack you*: they overwrite the game's pedestrian tables, so undoing them needs a save reload.
- **What's on?** The **Active** tab (its title shows the count) lists every trainer feature and game cheat currently on, plus locked weather, changed game speed and changed density. Tick **Settings → Active cheats list** to also show the list on screen while playing.
- **Spawning:** pick a category, select a vehicle, press **Spawn** (or double-click it). With *Put me in it* ticked you start in the driver's seat. Boats appear in front of you, so stand near water. Trains, the police helicopter and the Dodo aren't listed because the player can't drive them.
- **Teleporting:** after a teleport the help box says if you were moved to a clear spot, or why a teleport was cancelled (only water, or no open street for a vehicle). Pickups are walked onto when you're on foot, so they're collected.
- **Saved locations** are stored in `VCTrainer_positions.txt`, one per line (`x y z name`), so you can edit or share them.
- **Repair** restores health, engine and tyres. Body dents stay, the same as the game's own repair cheat.
- **Heavy car:** with *Blow it up*, also tick **Car god mode**, because the blasts are right next to you.
- **Genocide mode** kills mission characters too, so it will fail missions; keep infinite health on.
- **Never fall off bike:** if you ever end up stuck on a bike, hold the exit key for 1.5 s to be taken off.

### Hotkeys

Open the **Hotkeys** tab. Every feature in the trainer is listed there, grouped by
category (General, Player, Money, Vehicle, World, Teleport, Overlay, Give weapon,
Game cheats).

- **Bind:** click the key box next to a feature, then press the key or combination.
  - Hold **Ctrl**, **Shift** and/or **Alt** and press a key: `Ctrl + Shift + F`.
  - Tap **Ctrl**, **Shift** or **Alt** on its own to bind just that key.
  - **Middle mouse** and the **side mouse buttons** work too.
- **Clear:** press **Esc** while binding, or click the **x** at the end of the row.
- Labels show the type: **on/off** toggles a setting, **hold** works while held
  (for example *Boost*), the rest run once per press.
- A combo only fires with exactly its modifiers held, so `F`, `Shift + F` and `Alt + F`
  can all do different things.
- An orange **!** marks two features sharing the same key; hover it to see which.
- Use the **search box** or **Only bound** to find things quickly. **Clear all** removes every binding.
- Hotkeys work in game while the menu is **closed**. Plain INSERT and F7 are reserved for the menu
  (`Ctrl + Insert` and similar are fine).
- When a toggle is switched by hotkey, the game's help box says so (e.g. *Fly mode: ON*).

### Fly mode

Turn it on in the **Player** tab or with its hotkey.

| Key | Movement |
|---|---|
| **W / S** | Forward / back in the direction the camera looks (look up and press W to climb) |
| **A / D** | Sideways |
| **Space** | Straight up |
| **Ctrl** | Straight down |
| **Shift** (hold) | 3× speed |

- Goes through walls, buildings and the ground; works in vehicles too.
- Speed is adjustable from 5 to 150 m/s (slider, or the *Fly speed +10 / −10* hotkeys).
- These keys are read directly, so they work even if you've remapped controls in the game options.
- On foot you can still fight: the fire button shoots, the **mouse wheel** switches weapons, and rockets and grenades leave in front of you.
- Nothing collides with you while flying (your own rockets and grenades pass through you); explosions next to you still hurt.
- Switching fly mode off in mid-air drops you, so keep **infinite health** on.

### Movement: super jump, super run, bunny hop and surf

All in the **Player** tab under *Movement*:

- **Super jump** and **Super run** with power / speed sliders. Super run stops at walls and ledges.
- **Auto bunny hop:** hold the jump key and you hop again the instant you land, CS:GO style (no take-off crouch, no landing). In the air, hold **A** or **D** and turn the mouse the same way to **air strafe** (CS:GO's air-acceleration numbers). Modes: *Any direction*, *AFH* (gain while going forward) and *ABH* (gain while going backward), with sliders for gain per hop, top speed and hop height. Walls slide you along them. Let go of jump to land normally.
- **Surf on steep ramps:** land on a slope steeper than about 45° and you slide along it with no friction; hold A or D into the ramp to stay on it.
- **CS:GO feel** puts every hop setting back to CS:GO's own values.
- **No landing roll:** no forward roll, crouch or slow get-up after a fall.

### Combat: aim lock, enemy markers and weapon options

- **Aim lock-on** (Player tab): hold right mouse to aim and the crosshair snaps onto the nearest enemy all round you. It keeps tracking moving targets (aiming ahead of them), works in scope views, and lets go when you stop aiming, the enemy dies, or you yank the mouse. Enemies are gang members, police, criminals, anyone attacking you, and police helicopters; your followers and passengers are never targeted. While **bunny hopping** the camera is never turned: holding fire picks the enemy nearest your crosshair and every shot lands on them (silent aim).
- **Show enemies through walls** (Player tab): boxes round every enemy, solid red when you can see them, dashed orange behind walls, with distance; arrows round the screen for enemies off screen. Hidden on the game's own menus.
- **Weapons tab:** *Wall bang* (bullets through walls), *One-shot vehicles*, *Rapid fire*, *No-scope sniper* (sniper, laser sniper and rocket launcher from the hip; with aim lock, hold left click to lock then fire), *Sniper full auto*.

### Master switch

The **TRAINER ON / TRAINER OFF** button at the top of the menu (and the
*Trainer on/off* hotkey under General) pauses **every** running cheat at once: health,
fly mode, proofs, never wanted, ammo, frozen time, the overlay and all other hotkeys.
Your choices are kept, so switching back on restores exactly what was on. Things that
already happened (money added, a car spawned, weather or game speed changed) are not
undone.

### Files the trainer writes

All in the game folder:

| File | Contents |
|---|---|
| `VCTrainer.ini` | Your toggles, hotkeys, sliders (fly, jump, run, hop), max health/armour. Saved when you close the menu |
| `VCTrainer_layout.ini` | Menu window size and position |
| `VCTrainer_positions.txt` | Saved teleport locations |

Delete them to reset the trainer to defaults.

> Back up any save you care about before saving with trainer changes active.

---

## Uninstalling

Delete `VCTrainer.asi` from the game folder (or `scripts`), and the three files above if you
want. Leave the ASI loader's `dinput8.dll` in place if other mods use it.
The trainer never modifies `gta-vc.exe` or any game file on disk; all changes happen in
memory while the game runs.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Warning *"this gta-vc.exe is not v1.0"* | Your exe is a different version. The trainer only supports v1.0 (3,088,896 bytes). |
| Nothing happens when pressing INSERT/F7 | Load a save first: the menu works only during gameplay. Check `VCTrainer.asi` sits next to `gta-vc.exe` (or in `scripts`) and Ultimate ASI Loader is installed. |
| No "Modern VC Trainer loaded" message | The trainer wasn't loaded. Check Ultimate ASI Loader's `dinput8.dll` is the 32-bit one and sits next to `gta-vc.exe`, and that `VCTrainer.asi` ends in `.asi` (not `.asi.dll`; turn on file extensions in Explorer). Make sure it isn't blocked (right-click → Properties → *Unblock*). |
| Menu cursor too slow / fast | **Settings** → *Mouse speed*. |
| A hotkey does nothing | Check the master switch is ON, the menu is closed, and there's no orange **!** clash. |
| Another mod stopped working, or the ASI loader got replaced | Only one file can be called `dinput8.dll` - it must be the ASI loader's. If an old trainer `dinput8.dll` is there, replace it with the loader's and use `VCTrainer.asi`. |
| *"another mod has replaced the game's main-loop call"* | A mod changed that spot in a way the trainer can't chain through. Remove mods one at a time to find which one, and please open an issue. |
| Spawned boat is on land | Boats spawn in front of you; stand at the water's edge. |

---

## How it works

The trainer is a single 32-bit DLL, shipped as `VCTrainer.asi`.

### 1. Getting loaded

An ASI loader calls `LoadLibrary` on every `.asi` file it finds, which runs our `DllMain` and
installs the hooks. (The DLL still carries a `DirectInput8Create` pass-through export from the days
it could also be installed as `dinput8.dll`; with an ASI loader it is simply unused.)

### 2. Hooks into the game

All addresses were found by disassembling the v1.0 exe and are listed with notes in
[`src/game.h`](src/game.h). The game version is checked first by the build stamp in the
exe's header (not by code bytes, which other mods may have patched); on another version
nothing is patched.

Every hook **chains**, so the trainer coexists with other mods: a redirected call keeps the
address that was there before (the game's function or another mod's hook) and calls it; the
bike hook calls through an existing jump if another mod put one there; the `Present` and
window-procedure hooks call the previous pointer. A per-process mutex makes sure only one
copy of the trainer hooks the game if it's installed twice.

| Hook | Where | Why |
|---|---|---|
| Main loop → `CGame::Process` (0x4A5DA0) | call redirected | Runs once per game frame on the game's own thread. Menu actions are queued and executed here, and every continuous cheat (god mode, fly, proofs, timers…) is applied right after the game's own update. |
| `CGame::Process` → `CPad::UpdatePads` (0x4A4412) | call redirected | Right after the game reads input: while the menu is open, the mouse movement is fed to ImGui and the game's pad and mouse state are blanked, so the player, camera and weapons ignore input. |
| `IDirect3DDevice8::Present` | vtable entry | Draws the ImGui menu and overlay on top of the finished frame. |
| Game window procedure | subclassed | Menu toggle key, keyboard and text input for ImGui, and hotkey capture. |
| `CBike::KnockOffRider` (0x613920) | inline jump + trampoline | *Never fall off bike*: knock-offs from crashes and hits are refused, while peds pulling you off and your own exit still work. |
| `CWanted::UpdateWantedLevel` (0x4D2110) | inline jump + trampoline | *Never wanted* holds the chaos at zero; the *wanted lock* clamps the stars to your maximum the moment the game recalculates them. |
| `CPed::SetArrestPlayer` (0x4EB5F0) | inline jump + trampoline | No busts while never wanted is on, on foot or pulled out of a car. |
| `CVehicle::KillPedsInVehicle` (0x5B8560) | inline jump + trampoline | *Survive vehicle explosions*: the player is left out of the kill (stay in the wreck) or put beside it first. |
| `CPed::SetFall` (0x4FD9F0) | inline jump + trampoline | *No ragdoll*: knock-downs of the player are refused. |

### 3. Drawing ImGui on Direct3D 8

Vice City renders with Direct3D 8, which Dear ImGui has no official backend for, so the
project includes its own small renderer ([`src/imgui_impl_dx8.cpp`](src/imgui_impl_dx8.cpp)).
D3D8 has no scissor test, so each draw command is clipped by setting the viewport to its
clip rectangle with a matching orthographic projection. Render state is saved and restored
with a state block, so the game's rendering is untouched. The font texture is a managed
texture, so it survives alt-tab / device resets.

### 4. Mouse input

The game reads the mouse through DirectInput and hides the Windows cursor, so the menu
cursor is driven by the game's own raw mouse deltas (undoing the invert-mouse setting)
and ImGui draws its own cursor.

### 5. Features

Most features use the game's own code rather than reimplementing it:

- **Native cheats** are triggered by feeding the cheat code to the game's own cheat checker
  (`CPad::AddToPCCheatString`), exactly as if it had been typed. The codes were decoded
  from the obfuscated table in the exe.
- **Weapons** load the weapon model through the streaming system and call `CPed::GiveWeapon`.
- **Vehicle spawning** loads the model, allocates the vehicle with the game's allocator and
  calls the right constructor for cars, bikes or boats, then adds it to the world, the same
  way the game's own script command does. Names come from the game's text (GXT) via
  `data/default.ide`.
- **Proofs** set the same flags the game's mission scripts use (`SET_CAR_PROOFS` /
  `SET_CHAR_PROOFS`).
- **Infinite sprint / fast reload** set the player flags used by script opcodes 0330 / 0331.
- **Warp into vehicle** uses `CPed::SetObjective` + `CPed::WarpPedIntoCar`, like script opcode 0369.
- **Teleport** works like the game's own script teleport: the destination's collision and scene are
  loaded first, the ground is found, then the spot is checked with the game's line-of-sight tests,
  `CWorld::FindObjectsKindaColliding`, the water map and a sky check, and the entity is moved with its
  own `Teleport`. Radar blips come from `CRadar::ms_RadarTrace`, pickups from `CPickups::aPickUps`.
- **Aim lock** turns the active camera's angles (`CCam` Alpha/Beta) so the game's own crosshair
  (`CCamera::Find3rdPersonCamTargetVector`) lands on the target; enemy markers are projected with
  `CSprite::CalcScreenCoors`.
- **Weapon features** follow each shot by the weapon's fire timer and fire extra shots with the game's
  own `CWeapon::Fire` / `FireInstantHit` / `FireProjectile`; police helicopters (`CHeli`) are shot down
  through their own *shot down* state.
- **Bunny hop and surf** move the player themselves each frame (Source-style air acceleration and
  velocity clipping against ramps found with `CWorld::ProcessLineOfSight`), while the game keeps the
  jump arc and animations (`JUMP_glide`) are held with the game's animation blender.
- **Fly mode** places the player (or vehicle) each frame at a position the trainer moves along
  the camera's look direction, after the game's physics, and re-files the entity in the world's
  sector lists so it keeps rendering wherever it goes. That's why it ignores walls and gravity.
  The entity's collision flag is off while flying, so your own projectiles pass through you.
- **Mission timers** are frozen by preventing the on-screen countdown's script variable from decreasing; rampage timers by holding `CDarkel`'s start time.
- **Heavy car** sets your car's mass and turn mass to the Rhino's (18,000 kg, from `handling.cfg`), so the game's own collision physics does the shoving. Touches are read from the game's own collision records; *push* sets the other car's velocity away from you, *blow up* calls the vehicle's own `BlowUpCar`, like the BIGBANG cheat.
- **Bike exit in mid-air:** if you press exit and the game doesn't start getting you off within 0.2 s (it refuses while airborne), the trainer calls the game's knock-off routine itself. It respects the game's *player controls disabled* flag, so cutscenes aren't affected.
- **Camera** is put straight behind the player with the game's own
  `CCamera::SetCameraDirectlyBehindForFollowPed` when the menu opens.

---

## Building from source

The release DLL is all most people need. To build it yourself:

1. Install [Zig](https://ziglang.org/) (it includes a C++ compiler that can target 32-bit Windows):
   ```
   winget install zig.zig
   ```
2. Clone with the Dear ImGui submodule:
   ```
   git clone --recurse-submodules https://github.com/Sahil12524/GTAVC-Classic-ModernTrainer.git
   ```
   (Already cloned without it? Run `git submodule update --init`.)
3. Run `build.bat`. It produces `VCTrainer.asi` in the project folder.
4. Copy `VCTrainer.asi` next to `gta-vc.exe` (with Ultimate ASI Loader installed).

The build is a single command (see [`build.bat`](build.bat)):
`zig c++ -target x86-windows-gnu -std=c++17 -O2 -shared ...` compiling the sources in
`src/` together with Dear ImGui. Compiler flags `IMGUI_USE_BGRA_PACKED_COLOR` (vertex colours
match D3D8's format) and `IMGUI_DISABLE_OBSOLETE_FUNCTIONS` are required.

---

## Project layout

```
src/
  dllmain.cpp         startup, version check, single-instance guard
  hooks.cpp           game, input, window and Direct3D 8 hooks; ImGui frame
  features.cpp/.h     every cheat, the hotkey registry, settings file
  menu.cpp            the ImGui menu (tabs, styling, overlay)
  tables.h            weapon list and decoded native cheat list
  game.h              all game addresses and structure offsets, with notes
  imgui_impl_dx8.cpp  Dear ImGui renderer for Direct3D 8
third_party/imgui     Dear ImGui v1.91.8 (git submodule)
build.bat             build script
```

Adding a feature usually means: an address or offset in `game.h`, the logic in
`features.cpp`, a control in `menu.cpp`, and one line in `HotkeyActions()` so it can be
bound to a key.

---

## Credits and licence

- Trainer code: MIT licence, see [LICENSE](LICENSE).
- [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut and contributors, MIT licence.
- Grand Theft Auto and Vice City are trademarks of Take-Two Interactive Software. This project
  is not affiliated with them and includes no game assets or code from the game.
