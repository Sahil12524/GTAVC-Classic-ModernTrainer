#pragma once

extern bool  g_menuOpen;
extern float g_mouseSensitivity;

bool InstallHooks();               // hooks.cpp
void ForceOffBike(void* bike, void* ped); // hooks.cpp, game's own knock-off, no re-seat
void BikeKnockedOff(void* bike);          // features.cpp, called by the knock-off hook
bool BikeExitRequested();                 // exit pressed on a bike in the last 2 s
bool HoldNoWanted();
int WantedCap();                        // features.cpp: highest stars allowed, -1 = no limit
bool NoRagdollOn();                     // features.cpp: player can't be knocked down
bool SurviveBlowUpOn();                   // features.cpp: explosion hook may take the player out
void EjectPlayerFromBlast(void* veh);     // features.cpp: player out of an exploding vehicle, alive
bool BlowUpStayInside();                  // features.cpp: survive mode 0 = stay in the wreck
void** PlayerSeatInBlast(void* veh);      // features.cpp: the player's seat slot in veh (protects the player)                      // features.cpp: wanted hook keeps the player at 0 stars
void ApplyTrainerStyle();          // menu.cpp
void DrawTrainerMenu();            // menu.cpp (render thread = game thread)
void TrainerTick();                // features.cpp, once per game frame
void ShowNative(const char* fmt, ...); // features.cpp, game's own help box
