#ifdef __3DS__

#include "Platform3DS.h"

#include <3ds.h>
#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <malloc.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>

// Main thread stack. The default is far too small for this game.
unsigned int __stacksize__ = 1024 * 1024;

// Memory: keep libctru's default heap/linear split. (Do NOT override __ctru_heap_size: the malloc
// heap lives in a 96 MB virtual window, a bigger request makes the app panic at launch.)
// The unused part of the linear heap is used by `new` once malloc runs out - see Alloc3DS.cpp.

static bool s_isNew3DS = false;

// Defined further down; used by Platform3DS_Init()
static void OnUncaughtException();
static void OnOutOfMemory();
static void OnCpuException(ERRF_ExceptionInfo* info, CpuRegisters* regs) __attribute__((noreturn));
extern u32 s_exceptionStack[4096];

static const char* kDataDir = "sdmc:/3ds/OpenClaw";

void Platform3DS_Init()
{
    Alloc3DS_InstallSdlHooks();

    // New 3DS: unlock the faster CPU + extra L2 cache
    APT_CheckNew3DS(&s_isNew3DS);
    if (s_isNew3DS)
    {
        osSetSpeedupEnable(true);
    }

    mkdir("sdmc:/3ds", 0777);
    mkdir(kDataDir, 0777);
    mkdir("sdmc:/3ds/OpenClaw/tmp", 0777);
    chdir(kDataDir);

    // Everything the game prints (SDL_Log / LOG / LOG_ERROR go to stderr, printf to stdout)
    // is written to sdmc:/3ds/OpenClaw/log.txt so failures can be diagnosed without a PC link.
    // The file is recreated on every launch.
    if (freopen("sdmc:/3ds/OpenClaw/log.txt", "w", stderr))
    {
        setvbuf(stderr, NULL, _IONBF, 0);
        stdout = stderr; // same file, same ordering, no buffering delay
        fprintf(stderr, "OpenClaw 3DS log - %s 3DS, cwd=%s\n", s_isNew3DS ? "New" : "Old", kDataDir);
    }

    // Make out-of-memory and uncaught exceptions leave a readable trace in log.txt
    std::set_terminate(OnUncaughtException);
    std::set_new_handler(OnOutOfMemory);
    // CPU exceptions (data abort, ...) get logged with pc/lr before the process exits
    threadOnException(OnCpuException, &s_exceptionStack[4096], WRITE_DATA_TO_HANDLER_STACK);

    Platform3DS_LogMemory("startup");
}

void Platform3DS_Shutdown()
{
}

void Platform3DS_LogMemory(const char* tag)
{
    struct mallinfo mi = mallinfo();
    fprintf(stderr, "[mem] %-30s heap in use %6d KB of %6d KB | app region free %6u KB | linear free %6u KB\n",
        tag, mi.uordblks / 1024, mi.arena / 1024,
        (unsigned)(osGetMemRegionFree(MEMREGION_APPLICATION) / 1024),
        (unsigned)(linearSpaceFree() / 1024));
}

// ---------------------------------------------------------------------------
// Frame profiler
// ---------------------------------------------------------------------------
static const int kProfSlots = 8;
static const char* kProfNames[kProfSlots] = {
    "input", "events", "physics", "viewUpd", "actorUpd", "draw", "present", "frame" };
static double s_profAcc[kProfSlots];
static int s_profFrames = 0;
static double s_profLastFrame = 0.0;

double Platform3DS_NowMs()
{
    return (double)svcGetSystemTick() / (double)SYSCLOCK_ARM11 * 1000.0;
}

void Platform3DS_ProfileAdd(int slot, double ms)
{
    if (slot >= 0 && slot < kProfSlots)
    {
        s_profAcc[slot] += ms;
    }
}

void Platform3DS_ProfileEndFrame()
{
    double now = Platform3DS_NowMs();
    if (s_profLastFrame != 0.0)
    {
        s_profAcc[7] += now - s_profLastFrame;
        s_profFrames++;
    }
    s_profLastFrame = now;

    if (s_profFrames >= 120)
    {
        char line[256];
        int n = snprintf(line, sizeof(line), "[prof] %.1f fps |", 1000.0 / (s_profAcc[7] / s_profFrames));
        for (int i = 0; i < kProfSlots - 1 && n < (int)sizeof(line); i++)
        {
            n += snprintf(line + n, sizeof(line) - n, " %s %.1f", kProfNames[i], s_profAcc[i] / s_profFrames);
        }
        char stats[96];
        Platform3DS_FormatDrawStats(stats, sizeof(stats), s_profFrames);
        fprintf(stderr, "%s (ms/frame) | %s\n", line, stats);
        for (int i = 0; i < kProfSlots; i++)
        {
            s_profAcc[i] = 0.0;
        }
        s_profFrames = 0;
    }
}

// ---------------------------------------------------------------------------
// CPU exception handler: a data/prefetch abort writes its details to log.txt, then the process exits.
// Look the address up with:  arm-none-eabi-addr2line -f -C -e OpenClaw.elf 0x<pc>
// ---------------------------------------------------------------------------
u32 s_exceptionStack[4096] __attribute__((aligned(8))); // 16 KB private stack for the handler

static void OnCpuException(ERRF_ExceptionInfo* info, CpuRegisters* regs) __attribute__((noreturn));
static void OnCpuException(ERRF_ExceptionInfo* info, CpuRegisters* regs)
{
    fprintf(stderr, "\n[crash] CPU exception type %d (0=prefetch abort 1=data abort 2=undefined 3=vfp)\n", (int)info->type);
    fprintf(stderr, "[crash] fsr 0x%08x  far (fault address) 0x%08x\n", (unsigned)info->fsr, (unsigned)info->far);
    fprintf(stderr, "[crash] pc 0x%08x  lr 0x%08x  sp 0x%08x  cpsr 0x%08x\n",
        (unsigned)regs->pc, (unsigned)regs->lr, (unsigned)regs->sp, (unsigned)regs->cpsr);
    for (int i = 0; i < 13; i += 4)
    {
        fprintf(stderr, "[crash] r%-2d 0x%08x  r%-2d 0x%08x  r%-2d 0x%08x  r%-2d 0x%08x\n",
            i, (unsigned)regs->r[i], i + 1, (unsigned)regs->r[i + 1],
            i + 2, (unsigned)(i + 2 < 13 ? regs->r[i + 2] : 0), i + 3, (unsigned)(i + 3 < 13 ? regs->r[i + 3] : 0));
    }
    Platform3DS_LogMemory("at crash");
    fflush(stderr);
    svcExitProcess();
    for (;;) {}
}

static void OnUncaughtException()
{
    fprintf(stderr, "[fatal] std::terminate: uncaught C++ exception\n");
    try
    {
        std::exception_ptr ex = std::current_exception();
        if (ex)
        {
            std::rethrow_exception(ex);
        }
    }
    catch (const std::bad_alloc&)
    {
        fprintf(stderr, "[fatal] cause: std::bad_alloc (out of memory)\n");
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "[fatal] cause: %s\n", e.what());
    }
    catch (...)
    {
        fprintf(stderr, "[fatal] cause: unknown exception type\n");
    }
    Platform3DS_LogMemory("at terminate");
    abort();
}

static void OnOutOfMemory()
{
    fprintf(stderr, "[fatal] operator new failed - out of memory\n");
    Platform3DS_LogMemory("at out-of-memory");
    throw std::bad_alloc();
}

bool Platform3DS_IsNew3DS()
{
    return s_isNew3DS;
}

std::string Platform3DS_ConfigFileName()
{
    if (s_isNew3DS)
    {
        std::ifstream f("config_new3ds.xml");
        if (f.good())
        {
            return "config_new3ds.xml";
        }
    }
    return "config.xml";
}

// ---------------------------------------------------------------------------
// Input: 3DS buttons -> virtual keyboard keys.
//
// Game bindings (see ActorController.cpp):
//   arrows = move/climb/crouch, SPACE = jump, LALT = fire pistol/magic/dynamite,
//   LCTRL = sword attack, LSHIFT = change weapon, ESC = pause menu.
// A is only SPACE: the menus accept SPACE as "confirm". (Sending RETURN as well made one tap press the
// menu item twice and load the level twice.)
// ---------------------------------------------------------------------------

struct VirtualKey
{
    SDL_Keycode sym;
    SDL_Scancode scancode;
    u32 buttons; // any of these held -> key is down
    bool down;
};

static VirtualKey s_keys[] = {
    { SDLK_LEFT,   SDL_SCANCODE_LEFT,   KEY_DLEFT  | KEY_CPAD_LEFT,  false },
    { SDLK_RIGHT,  SDL_SCANCODE_RIGHT,  KEY_DRIGHT | KEY_CPAD_RIGHT, false },
    { SDLK_UP,     SDL_SCANCODE_UP,     KEY_DUP    | KEY_CPAD_UP,    false },
    { SDLK_DOWN,   SDL_SCANCODE_DOWN,   KEY_DDOWN  | KEY_CPAD_DOWN,  false },
    { SDLK_SPACE,  SDL_SCANCODE_SPACE,  KEY_A      | KEY_L,          false }, // jump
    { SDLK_LCTRL,  SDL_SCANCODE_LCTRL,  KEY_B      | KEY_R,          false }, // sword
    { SDLK_LALT,   SDL_SCANCODE_LALT,   KEY_X,                       false }, // fire
    { SDLK_LSHIFT, SDL_SCANCODE_LSHIFT, KEY_Y,                       false }, // change weapon
    { SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, KEY_START,                   false }, // pause
};

void Platform3DS_PumpInput()
{
    hidScanInput();
    const u32 held = hidKeysHeld();

    for (VirtualKey& k : s_keys)
    {
        const bool nowDown = (held & k.buttons) != 0;
        if (nowDown == k.down)
        {
            continue;
        }
        k.down = nowDown;

        SDL_Event e;
        SDL_zero(e);
        e.type = nowDown ? SDL_KEYDOWN : SDL_KEYUP;
        e.key.timestamp = SDL_GetTicks();
        e.key.state = nowDown ? SDL_PRESSED : SDL_RELEASED;
        e.key.repeat = 0;
        e.key.keysym.scancode = k.scancode;
        e.key.keysym.sym = k.sym;
        e.key.keysym.mod = KMOD_NONE;
        SDL_PushEvent(&e);
    }
}

#endif // __3DS__
