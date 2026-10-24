# OpenClaw for Nintendo 3DS (experimental, UNTESTED first cut)

This folder adds a devkitPro build of [OpenClaw](https://github.com/pjasicek/OpenClaw)
(a Captain Claw / Capitan Artiglio 1997 reimplementation). It has not been compiled or run
on hardware yet - expect a few build errors to fix on the first attempt.

## 1. Install the toolchain (on your PC)
Install devkitPro, then:

    sudo dkp-pacman -S 3ds-dev 3ds-sdl 3ds-sdl_mixer 3ds-sdl_image 3ds-sdl_ttf
    # Windows: use "dkp-pacman" from the devkitPro MSYS2 shell, same packages.
    export DEVKITPRO=/opt/devkitpro
    export DEVKITARM=$DEVKITPRO/devkitARM

## 2. Apply the patch + add the folder
From the root of your OpenClaw checkout (same level as `OpenClaw/`, `Box2D/`, `libwap/`):

    git apply 3ds/openclaw-3ds.patch      # edits inside OpenClaw/ (audio, config, input hook)

(The `3ds/` folder from the zip already sits at the repo root.)

## 3. Build

    cd 3ds
    make            # -> OpenClaw.3dsx
    make MUSIC=1    # optional: keep the SDL_mixer MIDI music path (heavy, likely too slow)

## 4. Put files on the SD card
Create `sdmc:/3ds/OpenClaw/` and copy:

- `OpenClaw.3dsx` -> `sdmc:/3ds/OpenClaw.3dsx` (or inside that folder)
- `config.xml` (Old 3DS, 16 MB resource cache) and `config_new3ds.xml` (New 3DS, 48 MB cache)
- **your own** `CLAW.REZ`
- `ASSETS.ZIP` = zip of the contents of `Build_Release/ASSETS/`
- `console02.tga` and `clacon.ttf` from `Build_Release/`
- `MENU.xml` / `SAVES.XML` from `Build_Release/` if the game complains

Launch from the Homebrew Launcher.

## Controls
| 3DS | Action |
|---|---|
| D-pad / Circle Pad | Move, climb, crouch |
| A or L | Jump (A also confirms in menus) |
| B or R | Sword attack |
| X | Fire pistol / magic / dynamite |
| Y | Change weapon |
| START | Pause menu |

Touch screen events go through OpenClaw's existing touch recognizers (pause tap, weapon tap, ...).

## What was changed
- Audio: music is OFF by default on 3DS (MIDI through SDL_mixer is too heavy); positional audio
  (`Mix_SetPosition` / `Mix_SetDistance`) is skipped, like the existing Emscripten branches.
- Input: `Platform3DS_PumpInput()` turns buttons into SDL key events.
- Config: Old 3DS uses `config.xml`; New 3DS uses `config_new3ds.xml` if present.
- SDL2_gfx (physics debug drawing) is stubbed out.
- Display: 400x240 window at scale 0.5.

## Known risks / first things to check
- Package names / SDL2 3DS backend version: if `SDL2_mixer` etc. fail to link, check `arm-none-eabi-pkg-config --static --libs sdl2 SDL2_mixer`.
- Renderer: SDL's 3DS renderer may be slow on Old 3DS; a citro2d backend could be added later.
- Memory: lower `ResourceCacheSize` if the game crashes while loading a level.
- 400x240 at scale 0.5 shows a wider view than the original 4:3; HUD placement may need tweaks.
