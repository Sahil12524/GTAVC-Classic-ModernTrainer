#pragma once

extern bool  g_menuOpen;
extern float g_mouseSensitivity;

bool InstallHooks();               // hooks.cpp
void ApplyTrainerStyle();          // menu.cpp
void DrawTrainerMenu();            // menu.cpp (render thread = game thread)
void TrainerTick();                // features.cpp, once per game frame
void ShowNative(const char* fmt, ...); // features.cpp, game's own help box
