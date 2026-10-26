#ifdef __3DS__

// Global operator new/delete with a spill-over into the 3DS "linear" heap.
//
// The malloc heap of a 3DS app lives in a fixed 96 MB virtual window, so it can never grow past
// ~90 MB no matter how the application memory is split. Level 1 needs more than that, while
// ~30 MB of linear memory sits unused. When malloc fails, allocations from C++ `new` (actors, XML
// tree, STL containers, ...) are served from the linear heap instead.
//
// Pointers are told apart on delete with osConvertVirtToPhys(), which only returns non-zero for
// addresses inside the linear heap (and other hardware-mapped areas, never the malloc heap).

#include <3ds.h>
#include <SDL2/SDL.h>

#include "Platform3DS.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <new>

// Linear memory kept free for SDL / audio / GPU buffers; never spill below this.
static const size_t kLinearReserve = 3 * 1024 * 1024;

static bool s_spillLogged = false;

// Every block served from the linear heap gets a 16-byte header holding its size (needed by realloc).
static const size_t kHdr = 16;

static inline bool IsLinearPtr(void* p)
{
    return osConvertVirtToPhys(p) != 0;
}

static void* TryAlloc(size_t n)
{
    if (n == 0)
    {
        n = 1;
    }

    void* p = malloc(n);
    if (p)
    {
        return p;
    }

    if ((size_t)linearSpaceFree() > n + kLinearReserve)
    {
        p = linearAlloc(n + kHdr);
        if (p)
        {
            if (!s_spillLogged)
            {
                s_spillLogged = true;
                fprintf(stderr, "[mem] malloc heap exhausted - spilling allocations into the linear heap\n");
            }
            *(size_t*)p = n;
            return (char*)p + kHdr;
        }
    }
    return nullptr;
}

static void* AllocOrThrow(size_t n)
{
    for (;;)
    {
        if (void* p = TryAlloc(n))
        {
            return p;
        }

        std::new_handler h = std::get_new_handler();
        if (!h)
        {
            throw std::bad_alloc();
        }
        h(); // our handler logs and throws std::bad_alloc
    }
}

static void FreePtr(void* p)
{
    if (!p)
    {
        return;
    }
    if (IsLinearPtr(p))
    {
        linearFree((char*)p - kHdr);
    }
    else
    {
        free(p);
    }
}

// ---- SDL allocations (surface/texture pixels are the biggest consumers) ----
static void* SdlMalloc(size_t n)
{
    void* p = TryAlloc(n);
    if (!p)
    {
        fprintf(stderr, "[fatal] SDL allocation of %u bytes failed (malloc heap and linear heap full)\n", (unsigned)n);
        Platform3DS_LogMemory("sdl alloc failed");
    }
    return p;
}

static void* SdlCalloc(size_t a, size_t b)
{
    void* p = SdlMalloc(a * b);
    if (p)
    {
        memset(p, 0, a * b);
    }
    return p;
}

static void* SdlRealloc(void* old, size_t n)
{
    if (!old)
    {
        return SdlMalloc(n);
    }
    if (!IsLinearPtr(old))
    {
        void* p = realloc(old, n);
        if (p)
        {
            return p;
        }
        // in-place growth failed: move to the linear heap (or fail)
        void* q = SdlMalloc(n);
        if (q)
        {
            // old block size is unknown for malloc blocks; n > old on a failed grow, copy what the
            // allocator says is usable
            size_t oldSize = malloc_usable_size(old);
            memcpy(q, old, oldSize < n ? oldSize : n);
            free(old);
        }
        return q;
    }
    size_t oldSize = *(size_t*)((char*)old - kHdr);
    if (n <= oldSize)
    {
        return old;
    }
    void* q = SdlMalloc(n);
    if (q)
    {
        memcpy(q, old, oldSize);
        FreePtr(old);
    }
    return q;
}

void Alloc3DS_InstallSdlHooks()
{
    SDL_SetMemoryFunctions(SdlMalloc, SdlCalloc, SdlRealloc, FreePtr);
}

void* operator new(std::size_t n)                                   { return AllocOrThrow(n); }
void* operator new[](std::size_t n)                                 { return AllocOrThrow(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept   { return TryAlloc(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return TryAlloc(n); }

void operator delete(void* p) noexcept                              { FreePtr(p); }
void operator delete[](void* p) noexcept                            { FreePtr(p); }
void operator delete(void* p, std::size_t) noexcept                 { FreePtr(p); }
void operator delete[](void* p, std::size_t) noexcept               { FreePtr(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept       { FreePtr(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept     { FreePtr(p); }

#endif // __3DS__
