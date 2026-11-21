#ifdef __3DS__

// Fast software rendering path for the 3DS.
//
// SDL's own software renderer draws straight into the window surface, whose format on the 3DS is
// RGBA8888. SDL has no optimised alpha blitter for that format, so every translucent pixel takes the
// generic path (~320 ns/px measured on a New 3DS, ~13x slower than an opaque copy).
// Instead we render into our own ABGR8888 surface (SDL *does* have a fast alpha blitter when texture and
// target share that layout) and convert it to the window surface once per frame with a byte swap.

#include "Platform3DS.h"
#include "Gpu3DS.h"

#include <SDL2/SDL.h>
#include <cstdio>
#include <unistd.h>

static SDL_Window* s_window = NULL;
static SDL_Surface* s_back = NULL;
static SDL_Renderer* s_renderer = NULL;

extern "C" void __real_SDL_RenderPresent(SDL_Renderer*);

SDL_Renderer* Platform3DS_CreateRenderer(SDL_Window* window)
{
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);

    // GPU path (citro2d). Creating the file sdmc:/3ds/OpenClaw/software_render.txt forces the old software renderer.
    bool useGpu = access("sdmc:/3ds/OpenClaw/software_render.txt", F_OK) != 0;
    if (useGpu && w == 400 && h == 240 && Gpu3DS_Init())
    {
        // The SDL renderer below is only a handle: the game passes it around and it stores the render scale.
        w = 1;
        h = 1;
    }
    else if (useGpu)
    {
        fprintf(stderr, "[gpu] not used (window %dx%d) - software renderer\n", w, h);
    }
    else
    {
        fprintf(stderr, "[gpu] software_render.txt found - software renderer\n");
    }

    s_back = SDL_CreateRGBSurface(0, w, h, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000); // ABGR8888
    if (!s_back)
    {
        return NULL;
    }

    s_renderer = SDL_CreateSoftwareRenderer(s_back);
    if (!s_renderer)
    {
        SDL_FreeSurface(s_back);
        s_back = NULL;
        return NULL;
    }

    s_window = window;
    if (!Gpu3DS_Active())
    {
        SDL_GetWindowSurface(window); // make sure the window framebuffer exists
    }
    return s_renderer;
}

static void CopyBackbufferToWindow()
{
    SDL_Surface* ws = SDL_GetWindowSurface(s_window);
    if (!ws)
    {
        return;
    }

    if (ws->format->format == SDL_PIXELFORMAT_RGBA8888 && ws->w == s_back->w && ws->h == s_back->h)
    {
        for (int y = 0; y < ws->h; y++)
        {
            const Uint32* src = (const Uint32*)((const Uint8*)s_back->pixels + y * s_back->pitch);
            Uint32* dst = (Uint32*)((Uint8*)ws->pixels + y * ws->pitch);
            for (int x = 0; x < ws->w; x++)
            {
                dst[x] = __builtin_bswap32(src[x]); // 0xAABBGGRR -> 0xRRGGBBAA
            }
        }
    }
    else
    {
        SDL_SetSurfaceBlendMode(s_back, SDL_BLENDMODE_NONE);
        SDL_BlitSurface(s_back, NULL, ws, NULL);
    }
    SDL_UpdateWindowSurface(s_window);
}

// Every SDL_RenderPresent in the game (linked with -Wl,--wrap=SDL_RenderPresent) lands here
extern "C" void __wrap_SDL_RenderPresent(SDL_Renderer* renderer)
{
    if (Gpu3DS_Active())
    {
        Gpu3DS_Present();
        return;
    }
    __real_SDL_RenderPresent(renderer); // executes the queued draw commands
    if (renderer == s_renderer && s_back)
    {
        CopyBackbufferToWindow();
    }
}

#endif // __3DS__
