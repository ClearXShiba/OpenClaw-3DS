#ifndef OPENCLAW_PLATFORM3DS_H
#define OPENCLAW_PLATFORM3DS_H

// Thin Nintendo 3DS platform layer for OpenClaw.
// Only compiled/used when building with devkitARM (__3DS__ is defined by the toolchain / Makefile).

#ifdef __3DS__

#include <string>

struct SDL_Renderer;
struct SDL_Window;

// Call first thing in main(): detects Old/New 3DS, enables New 3DS speedup,
// creates sdmc:/3ds/OpenClaw/ and makes it the working directory.
void Platform3DS_Init();

// Call right before exiting.
void Platform3DS_Shutdown();

// Call once per frame BEFORE SDL_PollEvent. Reads buttons + circle pad and pushes
// synthetic SDL_KEYDOWN / SDL_KEYUP events, so the existing keyboard code works unchanged.
void Platform3DS_PumpInput();

// "config_new3ds.xml" on a New 3DS (if that file exists), otherwise "config.xml".
std::string Platform3DS_ConfigFileName();

bool Platform3DS_IsNew3DS();

// Writes one line with heap / application-region / linear memory usage to the log (log.txt).
void Platform3DS_LogMemory(const char* tag);

// Lightweight frame profiler (averages are written to log.txt every 120 frames).
// Slots: 0 input/events, 1 event manager, 2 physics, 3 view update, 4 actor update,
//        5 scene draw, 6 present (vsync wait), 7 whole frame
double Platform3DS_NowMs();
void Platform3DS_ProfileAdd(int slot, double ms);
void Platform3DS_ProfileEndFrame();

struct Prof3DS
{
    int slot;
    double t0;
    explicit Prof3DS(int s) : slot(s), t0(Platform3DS_NowMs()) {}
    ~Prof3DS() { Platform3DS_ProfileAdd(slot, Platform3DS_NowMs() - t0); }
};

// Draw-call statistics ("copies N ex N scaled~ N px Nk" per frame; resets the counters) and a one-time
// blit benchmark of SDL's software renderer (results go to log.txt). See Bench3DS.cpp.
int Platform3DS_FormatDrawStats(char* buf, int n, int frames);
// Software renderer drawing into an ABGR8888 back buffer (fast alpha blits), converted to the window's
// RGBA8888 surface on SDL_RenderPresent. See Video3DS.cpp.
SDL_Renderer* Platform3DS_CreateRenderer(SDL_Window* window);
void Platform3DS_BlitBenchmark(SDL_Renderer* renderer, SDL_Window* window);

// Routes SDL's allocations through the malloc->linear-heap spill allocator (Alloc3DS.cpp).
// Must be called before SDL_Init.
void Alloc3DS_InstallSdlHooks();

#endif // __3DS__

#endif // OPENCLAW_PLATFORM3DS_H
