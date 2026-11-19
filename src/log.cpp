// VCTrainer.log, next to gta-vc.exe: one line per event, appended.
//
// Written the way VCModernFix writes its own log (config.cpp): each line is
// formatted in memory and written with a single WriteFile on a handle opened
// for appending, so a line always lands whole at the end of the file. The game
// runs the FPU at 24-bit precision; the C library's number formatting expects
// the default control word, which is set for the call and put back after.
// The file is kept under 1 MB: past that it is moved to VCTrainer.previous.log
// at start-up and a new one begun.
#include "trainer.h"
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void GamePath(char* out, const char* name) {
    char dir[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, dir, MAX_PATH);
    if (!n || n >= MAX_PATH) { lstrcpynA(out, name, MAX_PATH); return; }   // relative, as before
    char* slash = strrchr(dir, '\\');
    if (slash) slash[1] = 0; else dir[0] = 0;
    _snprintf(out, MAX_PATH, "%s%s", dir, name);
    out[MAX_PATH - 1] = 0;
}

static HANDLE g_log = INVALID_HANDLE_VALUE;
static bool g_logTried = false;

static HANDLE LogHandle() {
    if (g_log != INVALID_HANDLE_VALUE || g_logTried) return g_log;
    g_logTried = true;
    char path[MAX_PATH], prev[MAX_PATH];
    GamePath(path, "VCTrainer.log");
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &a) && (a.nFileSizeHigh || a.nFileSizeLow > 1024 * 1024)) {
        GamePath(prev, "VCTrainer.previous.log");
        MoveFileExA(path, prev, MOVEFILE_REPLACE_EXISTING);
    }
    g_log = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
    return g_log;
}

void Log(const char* fmt, ...) {
    unsigned short gameCw, defaultCw = 0x037F;
    __asm__ volatile("fnstcw %0" : "=m"(gameCw));
    __asm__ volatile("fldcw %0" : : "m"(defaultCw));
    char buf[1024];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int n = _snprintf(buf, sizeof buf, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(buf + n, sizeof buf - n - 2, fmt, ap);
    va_end(ap);
    if (m < 0) m = 0;
    if (m > (int)sizeof buf - n - 3) m = (int)sizeof buf - n - 3;   // cut a very long line
    n += m;
    buf[n++] = '\r';
    buf[n++] = '\n';
    HANDLE h = LogHandle();
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(h, buf, (DWORD)n, &written, nullptr);
    }
    __asm__ volatile("fldcw %0" : : "m"(gameCw));
}
