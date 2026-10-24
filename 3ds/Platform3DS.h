#ifndef OPENCLAW_PLATFORM3DS_H
#define OPENCLAW_PLATFORM3DS_H

// Thin Nintendo 3DS platform layer for OpenClaw.
// Only compiled/used when building with devkitARM (__3DS__ is defined by the toolchain / Makefile).

#ifdef __3DS__

#include <string>

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

#endif // __3DS__

#endif // OPENCLAW_PLATFORM3DS_H
