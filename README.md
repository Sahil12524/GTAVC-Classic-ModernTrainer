# GTAVC Classic Modern Trainer

An in-game trainer for the classic **GTA: Vice City (PC, v1.0)** with a modern
[Dear ImGui](https://github.com/ocornut/imgui) menu drawn inside the game.
No separate program to run: drop one DLL in the game folder, start the game,
press **INSERT** (or **F7**).

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
- Infinite health, infinite armour, never wanted, infinite ammo, no reload
- Infinite sprint, fast reload
- Bulletproof, fireproof, explosion-proof, collision-proof, melee-proof
- Health/armour sliders, max health and max armour up to 250
- Wanted level 0–6 stars, money (+$1K … +$1M, set any amount, take money)
- **Fly mode / no clip:** fly anywhere through walls and into the sky, on foot or in a vehicle

**Weapons**
- Give any of the 33 weapons with the ammo you choose
- The game's three weapon sets

**Vehicles**
- Spawn any car, bike, boat, helicopter or RC vehicle (searchable list with in-game names), optionally seated in it
- Repair, flip upright, lock/unlock doors, boost, jump, stop
- Car god mode, auto repair, bulletproof / fireproof / explosion-proof / solid (collision-proof) / melee-proof car
- **Never fall off bikes**
- Repaint from the game's own 95-colour palette
- Get into the nearest vehicle

**World**
- Set or freeze the time, force any weather, game speed (slow motion / fast)
- Pedestrian and traffic density
- Disable mission countdown timers

**Teleport**
- To the current mission marker, or any radar blip
- To typed coordinates (optionally snapped to the ground)
- Save your own locations and jump back to them

**Game cheats**
- All 58 built-in Vice City cheats, one click each (decoded straight from the game)

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

1. Download `dinput8.dll` from the [latest release](../../releases/latest).
2. Copy it into your Vice City folder, next to `gta-vc.exe`.
3. Start the game normally and load a save (or start a new game).
4. A message appears in the top-left help box: *"VC Trainer loaded. Press INSERT or F7 to open the menu."*
5. Press **INSERT** or **F7**.

That's it. There is nothing else to run and nothing is installed elsewhere on your PC.

> **Already using another mod that installs a `dinput8.dll`** (for example an ASI loader)?
> Only one file can have that name. Remove the other one, or build this trainer under a
> different loader-compatible name (see [Building from source](#building-from-source)).

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
| **Player** | Health/armour cheats, sprint, reload, proofs, fly mode, health & armour sliders, max health/armour, wanted stars, money |
| **Weapons** | Infinite ammo / no reload, the three weapon sets, a button for every weapon, custom ammo amount |
| **Vehicle** | Current-vehicle tools (repair, flip, doors, boost, jump, stop), protection options, never fall off bike, paint, and the vehicle spawner with category buttons and a search box |
| **World** | Time, freeze time, weather, game speed, pedestrian and traffic density, mission timers |
| **Teleport** | Your coordinates, teleport to coordinates, mission marker, radar blips list, saved locations |
| **Cheats** | All 58 built-in game cheats, grouped (player, weapons, weather, vehicle spawns, vehicles, pedestrians, skins). Hover a button to see the original code |
| **Hotkeys** | Bind any feature to a key (see below) |
| **Settings** | Mouse speed for the menu cursor, on-screen overlay, save settings |

Tips:
- **Spawning:** pick a category, select a vehicle, press **Spawn** (or double-click it). With *Put me in it* ticked you start in the driver's seat. Boats appear in front of you, so stand near water. Trains, the police helicopter and the Dodo aren't listed because the player can't drive them.
- **Teleport to mission marker** picks the first objective blip on the radar. The **All radar blips** list lets you choose any blip; map icons show as a number.
- **Saved locations** are stored in `VCTrainer_positions.txt`, one per line (`x y z name`), so you can edit or share them.
- **Repair** restores health, engine and tyres. Body dents stay, the same as the game's own repair cheat.

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
- While flying, the game's own walk/jump/fire actions are paused; the mouse still turns the camera.
- Switching fly mode off in mid-air drops you, so keep **infinite health** on.

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
| `VCTrainer.ini` | Your toggles, hotkeys, mouse and fly speed. Saved when you close the menu |
| `VCTrainer_layout.ini` | Menu window size and position |
| `VCTrainer_positions.txt` | Saved teleport locations |

Delete them to reset the trainer to defaults.

> Back up any save you care about before saving with trainer changes active.

---

## Uninstalling

Delete `dinput8.dll` from the game folder (and the three files above if you want).
The trainer never modifies `gta-vc.exe` or any game file on disk; all changes happen in
memory while the game runs.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Warning *"this gta-vc.exe is not v1.0"* | Your exe is a different version. The trainer only supports v1.0 (3,088,896 bytes). |
| Nothing happens when pressing INSERT/F7 | Load a save first: the menu works only during gameplay. Check `dinput8.dll` sits next to `gta-vc.exe`. |
| No "VC Trainer loaded" message | The DLL wasn't loaded. Make sure the file is named exactly `dinput8.dll` and isn't blocked (right-click → Properties → *Unblock*). |
| Menu cursor too slow / fast | **Settings** → *Mouse speed*. |
| A hotkey does nothing | Check the master switch is ON, the menu is closed, and there's no orange **!** clash. |
| Another mod stopped working | It probably also used `dinput8.dll`. Only one can be installed. |
| Spawned boat is on land | Boats spawn in front of you; stand at the water's edge. |

---

## How it works

The trainer is a single 32-bit DLL that the game loads by itself.

### 1. Getting loaded: the `dinput8.dll` proxy

`gta-vc.exe` imports `DirectInput8Create` from `dinput8.dll`. Windows looks in the game
folder before the system folder, so it loads ours. Our DLL exports the same function and
simply forwards every call to the real `dinput8.dll` from the system directory, so the
game's input works normally. Loading also runs our `DllMain`, which installs the hooks.

### 2. Hooks into the game

All addresses were found by disassembling the v1.0 exe and are listed with notes in
[`src/game.h`](src/game.h). Before patching, each hook site is checked against the bytes
it expects; if they don't match (another version), nothing is patched.

| Hook | Where | Why |
|---|---|---|
| Main loop → `CGame::Process` (0x4A5DA0) | call redirected | Runs once per game frame on the game's own thread. Menu actions are queued and executed here, and every continuous cheat (god mode, fly, proofs, timers…) is applied right after the game's own update. |
| `CGame::Process` → `CPad::UpdatePads` (0x4A4412) | call redirected | Right after the game reads input: while the menu is open, the mouse movement is fed to ImGui and the game's pad and mouse state are blanked, so the player, camera and weapons ignore input. |
| `IDirect3DDevice8::Present` | vtable entry | Draws the ImGui menu and overlay on top of the finished frame. |
| Game window procedure | subclassed | Menu toggle key, keyboard and text input for ImGui, and hotkey capture. |
| `CBike::KnockOffRider` (0x613920) | inline jump + trampoline | *Never fall off bike*: when the rider is the player, the call returns without throwing them off. |

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
- **Teleport** streams the destination in with `CStreaming::LoadScene` first, and can use
  `CWorld::FindGroundZForCoord` to land on the ground. Radar blips are read from `CRadar::ms_RadarTrace`.
- **Fly mode** places the player (or vehicle) each frame at a position the trainer moves along
  the camera's look direction, after the game's physics, and re-files the entity in the world's
  sector lists so it keeps rendering wherever it goes. That's why it ignores walls and gravity.
- **Mission timers** are frozen by preventing the on-screen countdown's script variable from decreasing.
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
   git clone --recurse-submodules https://github.com/<your-account>/GTAVC-Classic-ModernTrainer.git
   ```
   (Already cloned without it? Run `git submodule update --init`.)
3. Run `build.bat`. It produces `dinput8.dll` in the project folder.
4. Copy `dinput8.dll` next to `gta-vc.exe`.

The build is a single command (see [`build.bat`](build.bat)):
`zig c++ -target x86-windows-gnu -std=c++17 -O2 -shared ...` compiling the sources in
`src/` together with Dear ImGui. Compiler flags `IMGUI_USE_BGRA_PACKED_COLOR` (vertex colours
match D3D8's format) and `IMGUI_DISABLE_OBSOLETE_FUNCTIONS` are required.

---

## Project layout

```
src/
  dllmain.cpp         dinput8.dll proxy + startup
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
