#ifndef OPENCLAW_GPU3DS_H
#define OPENCLAW_GPU3DS_H

// GPU (citro2d) rendering path for the 3DS. See Gpu3DS.cpp.
//
// The game keeps calling SDL_RenderCopy / SDL_CreateTextureFromSurface / ... exactly as before.
// The linker (-Wl,--wrap=...) redirects those calls to Gpu3DS.cpp, which draws the sprites on the
// PICA200 GPU instead of copying pixels with the CPU. If the GPU path cannot start, everything falls back
// to the old SDL software renderer (Video3DS.cpp).

#ifdef __3DS__

#include <SDL2/SDL.h>

// Starts citro3d/citro2d and the top-screen render target. Returns false (and logs why) on failure.
bool Gpu3DS_Init();
bool Gpu3DS_Active();

// Ends the frame (SDL_RenderPresent).
void Gpu3DS_Present();

// SDL_RenderCopy / SDL_RenderCopyEx.
int Gpu3DS_RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect* src, const SDL_Rect* dst,
    double angle, const SDL_Point* center, int flip);

// Milliseconds spent waiting for the GPU / vblank since the last call (shown as gpuwait in the [prof] line).
double Gpu3DS_TakeWaitMs();

// One log line with texture counts / memory (written next to the [mem] lines).
void Gpu3DS_LogStats(const char* tag);

#endif // __3DS__

#endif // OPENCLAW_GPU3DS_H
