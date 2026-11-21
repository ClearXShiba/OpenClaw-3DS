#ifdef __3DS__

// GPU sprite rendering for the 3DS (citro2d on top of citro3d).
//
// Why: SDL's software renderer needs ~11 ms (draw) + ~11 ms (present) per frame in level 1 on a New 3DS,
// because the CPU copies every pixel. The PICA200 GPU does the same work for free, so the CPU is left with
// just the game logic.
//
// How: the engine keeps using the SDL_Renderer API. Linked with -Wl,--wrap=..., the calls below are
// intercepted. An SDL_Texture handed to the game is really a GpuTex (a swizzled GPU texture); the SDL_Renderer
// is still a tiny SDL software renderer that only serves as a handle and keeps the render scale.
//
// Textures: the GPU wants power-of-two sizes (8..1024) in an 8x8-tiled layout (Morton order). Sprites are padded up to the next power of two.
// When linear memory runs low, new textures are stored as 16 bit (RGBA5551, or RGBA4444 when they use
// partial transparency) instead of RGBA8.

#include "Gpu3DS.h"
#include "Platform3DS.h"

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>

// ---------------------------------------------------------------------------------------------
// The real SDL functions (the wrappers below replace the game's calls to them)
// ---------------------------------------------------------------------------------------------
extern "C"
{
SDL_Texture* __real_SDL_CreateTextureFromSurface(SDL_Renderer*, SDL_Surface*);
void __real_SDL_DestroyTexture(SDL_Texture*);
int __real_SDL_QueryTexture(SDL_Texture*, Uint32*, int*, int*, int*);
int __real_SDL_SetTextureColorMod(SDL_Texture*, Uint8, Uint8, Uint8);
int __real_SDL_SetTextureAlphaMod(SDL_Texture*, Uint8);
int __real_SDL_SetTextureBlendMode(SDL_Texture*, SDL_BlendMode);
int __real_SDL_RenderClear(SDL_Renderer*);
int __real_SDL_RenderFillRect(SDL_Renderer*, const SDL_Rect*);
int __real_SDL_RenderDrawPoint(SDL_Renderer*, int, int);
int __real_SDL_SetRenderDrawColor(SDL_Renderer*, Uint8, Uint8, Uint8, Uint8);
int __real_SDL_RenderReadPixels(SDL_Renderer*, const SDL_Rect*, Uint32, void*, int);
int __real_SDL_GetRendererInfo(SDL_Renderer*, SDL_RendererInfo*);
void __real_SDL_DestroyRenderer(SDL_Renderer*);
}

// ---------------------------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------------------------
static const int kScreenW = 400;
static const int kScreenH = 240;
static const int kMaxTexture = 1024;
// Below this much free linear memory, new textures are stored in 16 bit
static const unsigned kLowLinearBytes = 8u * 1024u * 1024u;
// Destroyed textures are kept until the GPU is known to be done with them; above this we force a sync
static const size_t kPendingSyncBytes = 2u * 1024u * 1024u;

static bool s_active = false;
static bool s_shutDown = false; // GPU path was used and has been closed: leftover textures are ours, never SDL's
static C3D_RenderTarget* s_target = NULL;
static bool s_inFrame = false;
static bool s_sceneBegun = false;

static Uint8 s_drawR = 0, s_drawG = 0, s_drawB = 0, s_drawA = 255;

struct GpuTex
{
    C3D_Tex tex;
    int w, h;      // size as the game sees it
    int pw, ph;    // power-of-two size on the GPU
    size_t bytes;  // linear memory used
    bool valid;    // false if the upload failed: draws are skipped
    Uint8 cr, cg, cb, ca; // SDL color / alpha mod
    int bpp;       // bytes per pixel on the GPU (4 or 2)
};

// Row order of the texture data. First hardware test showed the picture upside down with bottom-row-first storage,
// so the default is top-row-first. An empty file sdmc:/3ds/OpenClaw/gpu_flip.txt switches to the other order.
static bool s_flipRows = false;
static double s_waitMs = 0.0; // time spent waiting for the GPU / vertical blank in C3D_FrameBegin
static std::vector<GpuTex*> s_pending;
static size_t s_pendingBytes = 0;

static int s_texCount = 0;
static size_t s_texBytes = 0;
static int s_tex16Count = 0;
static int s_texFailed = 0;
static bool s_loggedUnsupported = false;

bool Gpu3DS_Active()
{
    return s_active;
}

// ---------------------------------------------------------------------------------------------
// Frame handling
// ---------------------------------------------------------------------------------------------
static void FreeTexNow(GpuTex* g)
{
    if (g->valid)
    {
        C3D_TexDelete(&g->tex);
        s_texBytes -= g->bytes;
    }
    s_texCount--;
    delete g;
}

static void ReleasePending()
{
    for (size_t i = 0; i < s_pending.size(); i++)
    {
        FreeTexNow(s_pending[i]);
    }
    s_pending.clear();
    s_pendingBytes = 0;
}

static void BeginFrame()
{
    if (s_inFrame)
    {
        return;
    }
    // Waits until the GPU finished the previous frame (and for the vertical blank)
    const double t0 = Platform3DS_NowMs();
    const bool began = C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    s_waitMs += Platform3DS_NowMs() - t0;
    if (began)
    {
        s_inFrame = true;
        s_sceneBegun = false;
        // The previous frame is complete, so textures destroyed meanwhile are no longer in use
        ReleasePending();
    }
}

static void BeginScene()
{
    BeginFrame();
    if (s_inFrame && !s_sceneBegun)
    {
        C2D_SceneBegin(s_target);
        s_sceneBegun = true;
    }
}

// Frees destroyed textures right now by waiting for the GPU (an empty frame draws nothing and shows nothing).
// Only possible between frames.
static void SyncAndRelease()
{
    if (s_inFrame || s_pending.empty())
    {
        return;
    }
    if (C3D_FrameBegin(C3D_FRAME_SYNCDRAW))
    {
        ReleasePending();
        C3D_FrameEnd(0);
    }
}

void Gpu3DS_Present()
{
    if (!s_active || !s_inFrame)
    {
        return;
    }
    C3D_FrameEnd(0);
    s_inFrame = false;
    s_sceneBegun = false;
}

// ---------------------------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------------------------
bool Gpu3DS_Init()
{
    if (s_active)
    {
        return true;
    }

    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE))
    {
        fprintf(stderr, "[gpu] C3D_Init failed - using the software renderer\n");
        return false;
    }
    if (!C2D_Init(2048))
    {
        fprintf(stderr, "[gpu] C2D_Init failed - using the software renderer\n");
        C3D_Fini();
        return false;
    }
    s_flipRows = access("sdmc:/3ds/OpenClaw/gpu_flip.txt", F_OK) == 0;
    if (s_flipRows)
    {
        fprintf(stderr, "[gpu] gpu_flip.txt found: textures stored bottom row first\n");
    }
    C2D_Prepare();
    C2D_SetTintMode(C2D_TintMult); // texture * (color, alpha): the same as SDL's color/alpha mod

    s_target = C3D_RenderTargetCreate(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_TOP, GPU_RB_RGBA8, GPU_RB_DEPTH16);
    if (!s_target)
    {
        fprintf(stderr, "[gpu] could not create the render target - using the software renderer\n");
        C2D_Fini();
        C3D_Fini();
        return false;
    }
    // SDL initialised the screens as RGBA8 (gfxInit(GSP_RGBA8_OES, ...)), so the transfer has to write RGBA8 too.
    C3D_RenderTargetSetOutput(s_target, GFX_TOP, GFX_LEFT,
        GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

    s_active = true;
    fprintf(stderr, "[gpu] citro2d renderer ready (top screen %dx%d)\n", kScreenW, kScreenH);
    return true;
}

static void Shutdown()
{
    if (!s_active)
    {
        return;
    }
    if (s_inFrame)
    {
        Gpu3DS_Present();
    }
    // Wait for the GPU, then free what is still queued
    if (C3D_FrameBegin(C3D_FRAME_SYNCDRAW))
    {
        ReleasePending();
        C3D_FrameEnd(0);
    }
    C2D_Fini();
    C3D_RenderTargetDelete(s_target);
    s_target = NULL;
    C3D_Fini();
    s_active = false;
    s_shutDown = true;
}

double Gpu3DS_TakeWaitMs()
{
    double v = s_waitMs;
    s_waitMs = 0.0;
    return v;
}

void Gpu3DS_LogStats(const char* tag)
{
    if (!s_active)
    {
        return;
    }
    fprintf(stderr, "[gpu] %-30s textures %d (%d in 16 bit, %d failed) %u KB linear, %u KB waiting to be freed\n",
        tag, s_texCount, s_tex16Count, s_texFailed, (unsigned)(s_texBytes / 1024), (unsigned)(s_pendingBytes / 1024));
}

// ---------------------------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------------------------
static int NextPow2(int v)
{
    int p = 8;
    while (p < v)
    {
        p <<= 1;
    }
    return p;
}

// Position of pixel (x, y) (both 0..7) inside an 8x8 tile: bits interleaved, x first (Morton / Z-order)
static inline unsigned Morton8(unsigned x, unsigned y)
{
    return (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
}

static bool HasPartialAlpha(const SDL_Surface* s)
{
    for (int y = 0; y < s->h; y++)
    {
        const Uint32* row = (const Uint32*)((const Uint8*)s->pixels + y * s->pitch);
        for (int x = 0; x < s->w; x++)
        {
            Uint32 a = row[x] & 0xff; // RGBA8888: alpha is the lowest byte of the 32 bit value
            if (a != 0 && a != 255)
            {
                return true;
            }
        }
    }
    return false;
}

static bool InitTexture(GpuTex* g, GPU_TEXCOLOR fmt)
{
    return C3D_TexInit(&g->tex, (u16)g->pw, (u16)g->ph, fmt);
}

// Writes the surface (RGBA8888 byte order R,G,B,A in memory => 32 bit value 0xRRGGBBAA) into the GPU texture
template <typename PixelT, typename ConvertFn>
static void Upload(GpuTex* g, const SDL_Surface* s, ConvertFn convert)
{
    PixelT* dst = (PixelT*)g->tex.data;
    memset(dst, 0, (size_t)g->pw * g->ph * sizeof(PixelT)); // padding stays transparent
    const int tilesPerRow = g->pw >> 3;
    for (int y = 0; y < s->h; y++)
    {
        const Uint32* src = (const Uint32*)((const Uint8*)s->pixels + y * s->pitch);
        const int memRow = s_flipRows ? g->ph - 1 - y : y;
        PixelT* rowBase = dst + (size_t)(memRow >> 3) * tilesPerRow * 64;
        const unsigned ty = memRow & 7;
        for (int x = 0; x < s->w; x++)
        {
            rowBase[(size_t)(x >> 3) * 64 + Morton8(x & 7, ty)] = convert(src[x]);
        }
    }
}

static inline u32 Conv8888(Uint32 p) { return p; }
static inline u16 Conv5551(Uint32 p)
{
    unsigned r = p >> 24, gg = (p >> 16) & 255, b = (p >> 8) & 255, a = p & 255;
    return (u16)(((r >> 3) << 11) | ((gg >> 3) << 6) | ((b >> 3) << 1) | (a >= 128 ? 1 : 0));
}
static inline u16 Conv4444(Uint32 p)
{
    unsigned r = p >> 24, gg = (p >> 16) & 255, b = (p >> 8) & 255, a = p & 255;
    return (u16)(((r >> 4) << 12) | ((gg >> 4) << 8) | ((b >> 4) << 4) | (a >> 4));
}

static GpuTex* CreateGpuTexture(SDL_Surface* surface)
{
    GpuTex* g = new GpuTex();
    memset(&g->tex, 0, sizeof(g->tex));
    g->w = surface->w;
    g->h = surface->h;
    g->pw = NextPow2(g->w);
    g->ph = NextPow2(g->h);
    g->valid = false;
    g->bytes = 0;
    g->bpp = 4;
    g->cr = g->cg = g->cb = g->ca = 255;
    s_texCount++;

    if (g->w <= 0 || g->h <= 0)
    {
        return g; // empty surface (e.g. an empty text string): nothing to draw
    }
    if (g->pw > kMaxTexture || g->ph > kMaxTexture)
    {
        fprintf(stderr, "[gpu] texture %dx%d is too large for the GPU (max %d) - it will not be drawn\n", g->w, g->h, kMaxTexture);
        s_texFailed++;
        return g;
    }

    SDL_Surface* conv = surface;
    if (surface->format->format != SDL_PIXELFORMAT_RGBA8888)
    {
        conv = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0);
        if (!conv)
        {
            fprintf(stderr, "[gpu] could not convert a %dx%d surface to RGBA8888: %s\n", g->w, g->h, SDL_GetError());
            s_texFailed++;
            return g;
        }
    }

    // Choose the format. 32 bit while there is plenty of linear memory, otherwise 16 bit.
    bool low = linearSpaceFree() < kLowLinearBytes;
    GPU_TEXCOLOR fmt = GPU_RGBA8;
    bool partial = false;
    if (low)
    {
        partial = HasPartialAlpha(conv);
        fmt = partial ? GPU_RGBA4 : GPU_RGBA5551;
    }

    bool ok = InitTexture(g, fmt);
    if (!ok)
    {
        // Out of linear memory: free what the GPU is finished with, and retry (in 16 bit when it was 32)
        SyncAndRelease();
        if (fmt == GPU_RGBA8)
        {
            partial = HasPartialAlpha(conv);
            fmt = partial ? GPU_RGBA4 : GPU_RGBA5551;
        }
        ok = InitTexture(g, fmt);
    }

    if (!ok)
    {
        fprintf(stderr, "[gpu] out of linear memory for a %dx%d texture (%u KB free)\n", g->pw, g->ph,
            (unsigned)(linearSpaceFree() / 1024));
        s_texFailed++;
    }
    else
    {
        if (fmt == GPU_RGBA8)
        {
            g->bpp = 4;
            Upload<u32>(g, conv, Conv8888);
        }
        else if (fmt == GPU_RGBA5551)
        {
            g->bpp = 2;
            Upload<u16>(g, conv, Conv5551);
            s_tex16Count++;
        }
        else
        {
            g->bpp = 2;
            Upload<u16>(g, conv, Conv4444);
            s_tex16Count++;
        }
        g->bytes = (size_t)g->pw * g->ph * g->bpp;
        C3D_TexSetFilter(&g->tex, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&g->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        // No explicit cache flush: C3D_FrameEnd flushes the whole linear heap before the GPU reads it.
        g->valid = true;
        s_texBytes += g->bytes;
    }

    if (conv != surface)
    {
        SDL_FreeSurface(conv);
    }
    return g;
}

static inline GpuTex* AsTex(SDL_Texture* t)
{
    return (GpuTex*)t;
}

extern "C" SDL_Texture* __wrap_SDL_CreateTextureFromSurface(SDL_Renderer* r, SDL_Surface* surface)
{
    if (!s_active)
    {
        return __real_SDL_CreateTextureFromSurface(r, surface);
    }
    if (!surface)
    {
        SDL_SetError("Passed NULL surface");
        return NULL;
    }
    return (SDL_Texture*)CreateGpuTexture(surface);
}

extern "C" void __wrap_SDL_DestroyTexture(SDL_Texture* t)
{
    if (!s_active)
    {
        if (!s_shutDown)
        {
            __real_SDL_DestroyTexture(t);
        }
        return;
    }
    if (!t)
    {
        return;
    }
    GpuTex* g = AsTex(t);
    if (!g->valid)
    {
        FreeTexNow(g);
        return;
    }
    // The GPU may still be drawing from it: free after the next frame has started
    s_pending.push_back(g);
    s_pendingBytes += g->bytes;
    if (s_pendingBytes > kPendingSyncBytes)
    {
        SyncAndRelease();
    }
}

extern "C" int __wrap_SDL_QueryTexture(SDL_Texture* t, Uint32* format, int* access, int* w, int* h)
{
    if (!s_active)
    {
        return __real_SDL_QueryTexture(t, format, access, w, h);
    }
    if (!t)
    {
        return SDL_SetError("Invalid texture");
    }
    GpuTex* g = AsTex(t);
    if (format) *format = SDL_PIXELFORMAT_RGBA8888;
    if (access) *access = SDL_TEXTUREACCESS_STATIC;
    if (w) *w = g->w;
    if (h) *h = g->h;
    return 0;
}

extern "C" int __wrap_SDL_SetTextureColorMod(SDL_Texture* t, Uint8 r, Uint8 g, Uint8 b)
{
    if (!s_active)
    {
        return __real_SDL_SetTextureColorMod(t, r, g, b);
    }
    GpuTex* gt = AsTex(t);
    gt->cr = r;
    gt->cg = g;
    gt->cb = b;
    return 0;
}

extern "C" int __wrap_SDL_SetTextureAlphaMod(SDL_Texture* t, Uint8 a)
{
    if (!s_active)
    {
        return __real_SDL_SetTextureAlphaMod(t, a);
    }
    AsTex(t)->ca = a;
    return 0;
}

// Opaque sprites are drawn through the same blend path (their alpha is 255 anyway), so nothing to do
extern "C" int __wrap_SDL_SetTextureBlendMode(SDL_Texture* t, SDL_BlendMode mode)
{
    if (!s_active)
    {
        return __real_SDL_SetTextureBlendMode(t, mode);
    }
    return 0;
}

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------
static inline float RoundF(float v)
{
    return floorf(v + 0.5f);
}

int Gpu3DS_RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect* src, const SDL_Rect* dst,
    double angle, const SDL_Point* center, int flip)
{
    GpuTex* g = AsTex(texture);
    if (!g || !g->valid)
    {
        return 0;
    }

    float sx = 1.0f, sy = 1.0f;
    SDL_RenderGetScale(renderer, &sx, &sy);

    // Source rectangle, in texture pixels
    int srcX = 0, srcY = 0, srcW = g->w, srcH = g->h;
    if (src)
    {
        srcX = src->x;
        srcY = src->y;
        srcW = src->w;
        srcH = src->h;
        if (srcX < 0) { srcW += srcX; srcX = 0; }
        if (srcY < 0) { srcH += srcY; srcY = 0; }
        if (srcX + srcW > g->w) srcW = g->w - srcX;
        if (srcY + srcH > g->h) srcH = g->h - srcY;
        if (srcW <= 0 || srcH <= 0)
        {
            return 0;
        }
    }

    // Destination rectangle on the 400x240 screen (the game draws in logical units multiplied by the render scale)
    float dx0 = 0.0f, dy0 = 0.0f, dx1 = (float)kScreenW, dy1 = (float)kScreenH;
    if (dst)
    {
        dx0 = RoundF(dst->x * sx);
        dy0 = RoundF(dst->y * sy);
        dx1 = RoundF((dst->x + dst->w) * sx);
        dy1 = RoundF((dst->y + dst->h) * sy);
    }
    const float dw = dx1 - dx0, dh = dy1 - dy0;
    if (dw <= 0.0f || dh <= 0.0f)
    {
        return 0;
    }
    if (angle == 0.0 && (dx1 <= 0.0f || dy1 <= 0.0f || dx0 >= (float)kScreenW || dy0 >= (float)kScreenH))
    {
        return 0; // completely off screen
    }

    BeginScene();
    if (!s_sceneBegun)
    {
        return 0;
    }

    // Texture coordinates: v runs from 1 (image top) to 0 (image bottom).
    Tex3DS_SubTexture sub;
    sub.width = (u16)srcW;
    sub.height = (u16)srcH;
    sub.left = (float)srcX / g->pw;
    sub.right = (float)(srcX + srcW) / g->pw;
    sub.top = 1.0f - (float)srcY / g->ph;
    sub.bottom = 1.0f - (float)(srcY + srcH) / g->ph;
    if (flip & SDL_FLIP_HORIZONTAL)
    {
        float t = sub.left; sub.left = sub.right; sub.right = t;
    }
    if (flip & SDL_FLIP_VERTICAL)
    {
        float t = sub.top; sub.top = sub.bottom; sub.bottom = t;
    }

    C2D_Image image;
    image.tex = &g->tex;
    image.subtex = &sub;

    C2D_DrawParams params;
    params.pos.w = dw;
    params.pos.h = dh;
    params.depth = 0.0f;
    if (angle != 0.0)
    {
        // SDL rotates clockwise (degrees) around `center` (relative to the destination's top-left, default its middle)
        float cx = center ? center->x * sx : dw * 0.5f;
        float cy = center ? center->y * sy : dh * 0.5f;
        params.pos.x = dx0 + cx;
        params.pos.y = dy0 + cy;
        params.center.x = cx;
        params.center.y = cy;
        params.angle = (float)(angle * 3.14159265358979323846 / 180.0);
    }
    else
    {
        params.pos.x = dx0;
        params.pos.y = dy0;
        params.center.x = 0.0f;
        params.center.y = 0.0f;
        params.angle = 0.0f;
    }

    if (g->cr == 255 && g->cg == 255 && g->cb == 255 && g->ca == 255)
    {
        C2D_DrawImage(image, &params, NULL);
    }
    else
    {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(g->cr, g->cg, g->cb, g->ca), 1.0f);
        C2D_DrawImage(image, &params, &tint);
    }
    return 0;
}

extern "C" int __wrap_SDL_SetRenderDrawColor(SDL_Renderer* r, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca)
{
    s_drawR = cr;
    s_drawG = cg;
    s_drawB = cb;
    s_drawA = ca;
    return __real_SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
}

extern "C" int __wrap_SDL_RenderClear(SDL_Renderer* r)
{
    if (!s_active)
    {
        return __real_SDL_RenderClear(r);
    }
    BeginFrame();
    if (s_inFrame)
    {
        C2D_TargetClear(s_target, C2D_Color32(s_drawR, s_drawG, s_drawB, 255));
        BeginScene();
    }
    return 0;
}

extern "C" int __wrap_SDL_RenderFillRect(SDL_Renderer* r, const SDL_Rect* rect)
{
    if (!s_active)
    {
        return __real_SDL_RenderFillRect(r, rect);
    }
    float sx = 1.0f, sy = 1.0f;
    SDL_RenderGetScale(r, &sx, &sy);
    float x0 = 0.0f, y0 = 0.0f, x1 = (float)kScreenW, y1 = (float)kScreenH;
    if (rect)
    {
        x0 = RoundF(rect->x * sx);
        y0 = RoundF(rect->y * sy);
        x1 = RoundF((rect->x + rect->w) * sx);
        y1 = RoundF((rect->y + rect->h) * sy);
    }
    BeginScene();
    if (s_sceneBegun && x1 > x0 && y1 > y0)
    {
        C2D_DrawRectSolid(x0, y0, 0.0f, x1 - x0, y1 - y0, C2D_Color32(s_drawR, s_drawG, s_drawB, s_drawA));
    }
    return 0;
}

extern "C" int __wrap_SDL_RenderDrawPoint(SDL_Renderer* r, int x, int y)
{
    if (!s_active)
    {
        return __real_SDL_RenderDrawPoint(r, x, y);
    }
    float sx = 1.0f, sy = 1.0f;
    SDL_RenderGetScale(r, &sx, &sy);
    BeginScene();
    if (s_sceneBegun)
    {
        C2D_DrawRectSolid(RoundF(x * sx), RoundF(y * sy), 0.0f, 1.0f, 1.0f, C2D_Color32(s_drawR, s_drawG, s_drawB, s_drawA));
    }
    return 0;
}

// Reading the screen back is not supported on the GPU path (nothing in the game needs it): returns black
extern "C" int __wrap_SDL_RenderReadPixels(SDL_Renderer* r, const SDL_Rect* rect, Uint32 format, void* pixels, int pitch)
{
    if (!s_active)
    {
        return __real_SDL_RenderReadPixels(r, rect, format, pixels, pitch);
    }
    if (!s_loggedUnsupported)
    {
        fprintf(stderr, "[gpu] SDL_RenderReadPixels is not supported on the GPU renderer (returning black)\n");
        s_loggedUnsupported = true;
    }
    int w = rect ? rect->w : kScreenW;
    int h = rect ? rect->h : kScreenH;
    if (pixels && pitch > 0)
    {
        memset(pixels, 0, (size_t)pitch * h);
    }
    (void)w;
    return 0;
}

extern "C" int __wrap_SDL_GetRendererInfo(SDL_Renderer* r, SDL_RendererInfo* info)
{
    int rc = __real_SDL_GetRendererInfo(r, info);
    if (s_active && rc == 0 && info)
    {
        info->name = "citro2d";
        info->max_texture_width = kMaxTexture;
        info->max_texture_height = kMaxTexture;
    }
    return rc;
}

extern "C" void __wrap_SDL_DestroyRenderer(SDL_Renderer* r)
{
    Shutdown();
    __real_SDL_DestroyRenderer(r);
}

#endif // __3DS__
