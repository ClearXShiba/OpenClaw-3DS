#ifdef __3DS__

#include "Platform3DS.h"

#include <3ds.h>
#include <SDL2/SDL.h>

#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>

// Main thread stack. The default is far too small for this game.
unsigned int __stacksize__ = 1024 * 1024;

static bool s_isNew3DS = false;

static const char* kDataDir = "sdmc:/3ds/OpenClaw";

void Platform3DS_Init()
{
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
}

void Platform3DS_Shutdown()
{
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
// RETURN is also sent with A so menus can be confirmed with A.
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
    { SDLK_RETURN, SDL_SCANCODE_RETURN, KEY_A,                       false }, // menu confirm
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
