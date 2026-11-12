@echo off
rem Builds the trainer (32-bit) with Zig's bundled clang/MinGW toolchain into
rem VCTrainer.asi, a plugin for Ultimate ASI Loader.
rem Requires Zig (winget install zig.zig) and Dear ImGui in third_party\imgui.
cd /d "%~dp0"
set IMGUI=third_party\imgui
zig c++ -target x86-windows-gnu -std=c++17 -O2 -s -shared -fno-exceptions -fno-rtti ^
  -DIMGUI_USE_BGRA_PACKED_COLOR -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS -I%IMGUI% ^
  -o VCTrainer.asi ^
  src\dllmain.cpp src\hooks.cpp src\features.cpp src\menu.cpp src\imgui_impl_dx8.cpp ^
  %IMGUI%\imgui.cpp %IMGUI%\imgui_draw.cpp %IMGUI%\imgui_tables.cpp %IMGUI%\imgui_widgets.cpp ^
  -luser32 -lkernel32
if errorlevel 1 exit /b 1
del /q VCTrainer.pdb VCTrainer.lib dinput8.dll dinput8.pdb dinput8.lib dllmain.lib 2>nul
echo Built %~dp0VCTrainer.asi
