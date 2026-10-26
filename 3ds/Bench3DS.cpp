#ifdef __3DS__

// Diagnostics for SDL's software renderer on the 3DS:
//  - Platform3DS_BlitBenchmark(): one-time timing of RenderCopy variants at startup (written to log.txt)
//  - __wrap_SDL_RenderCopy / __wrap_SDL_RenderCopyEx: count draw calls (linked with -Wl,--wrap=...)

#include "Platform3DS.h"

#include <SDL2/SDL.h>

#include <cmath>
#include <cstdio>

// ---------------------------------------------------------------------------------------------
// Draw call statistics
// ---------------------------------------------------------------------------------------------
static unsigned s_copies = 0;
static unsigned s_copiesEx = 0;
static unsigned s_scaled = 0;
static double s_pixels = 0.0;

extern "C" int __real_SDL_RenderCopy(SDL_Renderer*, SDL_Texture*, const SDL_Rect*, const SDL_Rect*);
extern "C" int __real_SDL_RenderCopyEx(SDL_Renderer*, SDL_Texture*, const SDL_Rect*, const SDL_Rect*,
    const double, const SDL_Point*, const SDL_RendererFlip);

static void CountCopy(SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* src, const SDL_Rect* dst)
{
    if (!dst)
    {
        return;
    }

    float sx = 1.0f, sy = 1.0f;
    SDL_RenderGetScale(r, &sx, &sy);
    int tw = 0, th = 0;
    SDL_QueryTexture(t, NULL, NULL, &tw, &th);
    if (src)
    {
        tw = src->w;
        th = src->h;
    }

    int dw = (int)std::lround(dst->w * sx);
    int dh = (int)std::lround(dst->h * sy);
    s_pixels += (double)dw * (double)dh;
    if (std::abs(dw - tw) > 0 || std::abs(dh - th) > 0)
    {
        s_scaled++;
    }
}

extern "C" int __wrap_SDL_RenderCopy(SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* src, const SDL_Rect* dst)
{
    s_copies++;
    CountCopy(r, t, src, dst);
    return __real_SDL_RenderCopy(r, t, src, dst);
}

extern "C" int __wrap_SDL_RenderCopyEx(SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* src, const SDL_Rect* dst,
    const double angle, const SDL_Point* center, const SDL_RendererFlip flip)
{
    s_copiesEx++;
    CountCopy(r, t, src, dst);
    return __real_SDL_RenderCopyEx(r, t, src, dst, angle, center, flip);
}

int Platform3DS_FormatDrawStats(char* buf, int n, int frames)
{
    if (frames < 1)
    {
        frames = 1;
    }
    int len = snprintf(buf, n, "copies %u ex %u scaled~ %u px %uk",
        s_copies / frames, s_copiesEx / frames, s_scaled / frames, (unsigned)(s_pixels / frames / 1000.0));
    s_copies = s_copiesEx = s_scaled = 0;
    s_pixels = 0.0;
    return len;
}

// ---------------------------------------------------------------------------------------------
// Blit benchmark
// ---------------------------------------------------------------------------------------------
static SDL_Surface* MakeSprite(int w, int h, bool withAlpha)
{
    SDL_Surface* s = SDL_CreateRGBSurface(0, w, h, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, withAlpha ? 0xff000000 : 0);
    if (!s)
    {
        return NULL;
    }
    SDL_LockSurface(s);
    for (int y = 0; y < h; y++)
    {
        Uint32* row = (Uint32*)((Uint8*)s->pixels + y * s->pitch);
        for (int x = 0; x < w; x++)
        {
            Uint32 a = 255;
            if (withAlpha)
            {
                int cell = ((x * 8 / w) + (y * 8 / h)) & 3;
                a = (cell == 0) ? 0 : (cell == 1) ? 128 : 255; // transparent / translucent / opaque
            }
            Uint32 r = (x * 255) / w, g = (y * 255) / h, b = 128;
            row[x] = r | (g << 8) | (b << 16) | (withAlpha ? (a << 24) : 0);
        }
    }
    SDL_UnlockSurface(s);
    return s;
}

static double s_baseMs = 0.0;

static void RunCase(SDL_Renderer* r, const char* label, SDL_Texture* t, int dw, int dh)
{
    if (!t)
    {
        fprintf(stderr, "[bench] %-44s (texture missing)\n", label);
        return;
    }

    const int frames = 6;
    const int perFrame = 150;
    double t0 = Platform3DS_NowMs();
    for (int f = 0; f < frames; f++)
    {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderClear(r);
        for (int i = 0; i < perFrame; i++)
        {
            SDL_Rect d = { (i * 37) % (400 - dw), (i * 23) % (240 - dh), dw, dh };
            SDL_RenderCopy(r, t, NULL, &d);
        }
        SDL_RenderPresent(r);
    }
    double frameMs = (Platform3DS_NowMs() - t0) / frames;

    double usPerBlit = (frameMs - s_baseMs) / perFrame * 1000.0;
    double nsPerPixel = usPerBlit * 1000.0 / (dw * dh);
    fprintf(stderr, "[bench] %-44s %7.1f ms/frame  %7.1f us/blit  %6.1f ns/px\n", label, frameMs, usPerBlit, nsPerPixel);
}

void Platform3DS_BlitBenchmark(SDL_Renderer* r, SDL_Window* w)
{
    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(r, &info) == 0)
    {
        fprintf(stderr, "[bench] renderer '%s' flags 0x%x, window pixel format %s\n",
            info.name, (unsigned)info.flags, SDL_GetPixelFormatName(SDL_GetWindowPixelFormat(w)));
        fprintf(stderr, "[bench] texture formats:");
        for (Uint32 i = 0; i < info.num_texture_formats && i < 12; i++)
        {
            fprintf(stderr, " %s", SDL_GetPixelFormatName(info.texture_formats[i]));
        }
        fprintf(stderr, "\n");
    }

    float oldSx = 1.0f, oldSy = 1.0f;
    SDL_RenderGetScale(r, &oldSx, &oldSy);
    SDL_RenderSetScale(r, 1.0f, 1.0f);

    SDL_Surface* sprite64 = MakeSprite(64, 64, true);
    SDL_Surface* sprite32 = MakeSprite(32, 32, true);
    SDL_Surface* opaque200 = MakeSprite(200, 120, false);
    SDL_Texture* tAlpha = sprite64 ? SDL_CreateTextureFromSurface(r, sprite64) : NULL;
    SDL_Texture* tHalf = sprite32 ? SDL_CreateTextureFromSurface(r, sprite32) : NULL;
    SDL_Texture* tOpaque = opaque200 ? SDL_CreateTextureFromSurface(r, opaque200) : NULL;

    if (tAlpha)
    {
        Uint32 fmt = 0;
        int access = 0, tw = 0, th = 0;
        SDL_QueryTexture(tAlpha, &fmt, &access, &tw, &th);
        fprintf(stderr, "[bench] sprite texture format %s\n", SDL_GetPixelFormatName(fmt));
    }
    if (tOpaque)
    {
        Uint32 fmt = 0;
        SDL_QueryTexture(tOpaque, &fmt, NULL, NULL, NULL);
        fprintf(stderr, "[bench] opaque texture format %s\n", SDL_GetPixelFormatName(fmt));
    }

    // Texture in exactly the window's pixel format
    SDL_Texture* tMatched = NULL;
    Uint32 winFmt = SDL_GetWindowPixelFormat(w);
    if (sprite64 && winFmt != SDL_PIXELFORMAT_UNKNOWN)
    {
        if (SDL_Surface* conv = SDL_ConvertSurfaceFormat(sprite64, winFmt, 0))
        {
            tMatched = SDL_CreateTexture(r, winFmt, SDL_TEXTUREACCESS_STATIC, 64, 64);
            if (tMatched)
            {
                SDL_UpdateTexture(tMatched, NULL, conv->pixels, conv->pitch);
                SDL_SetTextureBlendMode(tMatched, SDL_BLENDMODE_BLEND);
            }
            SDL_FreeSurface(conv);
        }
    }

    // Empty frames (clear + present): the fixed cost every other case is measured against
    {
        const int frames = 6;
        double t0 = Platform3DS_NowMs();
        for (int f = 0; f < frames; f++)
        {
            SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
            SDL_RenderClear(r);
            SDL_RenderPresent(r);
        }
        s_baseMs = (Platform3DS_NowMs() - t0) / frames;
        fprintf(stderr, "[bench] %-44s %7.1f ms/frame\n", "true empty baseline", s_baseMs);
    }

    RunCase(r, "alpha 64x64 -> 64x64 (unscaled)", tAlpha, 64, 64);
    if (tAlpha)
    {
        SDL_SetTextureBlendMode(tAlpha, SDL_BLENDMODE_NONE);
        RunCase(r, "same, blend mode NONE", tAlpha, 64, 64);
        SDL_SetTextureBlendMode(tAlpha, SDL_BLENDMODE_BLEND);
    }
    RunCase(r, "alpha 64x64 -> 32x32 (scaled down)", tAlpha, 32, 32);
    RunCase(r, "alpha 32x32 -> 32x32 (half-size texture)", tHalf, 32, 32);
    RunCase(r, "alpha, texture in window pixel format", tMatched, 64, 64);
    if (tAlpha)
    {
        SDL_SetTextureColorMod(tAlpha, 250, 250, 250);
        RunCase(r, "alpha 64x64, color mod 250", tAlpha, 64, 64);
        SDL_SetTextureColorMod(tAlpha, 255, 255, 255);
        SDL_SetTextureAlphaMod(tAlpha, 200);
        RunCase(r, "alpha 64x64, alpha mod 200", tAlpha, 64, 64);
        SDL_SetTextureAlphaMod(tAlpha, 255);
    }
    RunCase(r, "opaque 200x120 (raw pixel rate)", tOpaque, 200, 120);

    if (tAlpha) SDL_DestroyTexture(tAlpha);
    if (tHalf) SDL_DestroyTexture(tHalf);
    if (tOpaque) SDL_DestroyTexture(tOpaque);
    if (tMatched) SDL_DestroyTexture(tMatched);
    if (sprite64) SDL_FreeSurface(sprite64);
    if (sprite32) SDL_FreeSurface(sprite32);
    if (opaque200) SDL_FreeSurface(opaque200);

    SDL_RenderSetScale(r, oldSx, oldSy);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    SDL_RenderPresent(r);

    // forget the statistics produced by the benchmark itself
    char tmp[64];
    Platform3DS_FormatDrawStats(tmp, sizeof(tmp), 1);
}

#endif // __3DS__
