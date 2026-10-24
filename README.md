# OpenClaw-3DS (Forked from OpenClaw by pjasicek, see https://github.com/pjasicek/OpenClaw) - Captain Claw (1997) reimplementation ported to the 3DS family of       systems.

 - This project is a C++ reimplementation port (+ some help from Claude Code, more info at the end) of the original OpenClaw to both New and Old 3DS.
 - Whole codebase was written from scratch by pjasicek (https://github.com/pjasicek).
 - Game uses assets from original game archive (CLAW.REZ)

**Level 1 youtube playthrough:**

[![OpenClaw - Level 1](https://img.youtube.com/vi/ikXQNV_aw68/0.jpg)](https://www.youtube.com/watch?v=ikXQNV_aw68)

# Build status

### 3DS:

The game is currently 10% complete; only the menu really works, and if  you try to boot into a level, the game will send you back to The Homebrew Loader.

Proof:

<img width="1084" height="945" alt="screen1" src="https://github.com/user-attachments/assets/38e6fb83-ad86-4f2f-b385-19dacf869fc4" />

Proof 2:

<img width="1033" height="945" alt="screen2" src="https://github.com/user-attachments/assets/afec1b9d-4ba7-41c2-b27e-fb0a3855918b" />


# Tech

  - SDL2 Libraries (SDL2, SDL_Image, SDL_TTF, SDL_Mixer, SDL2_Gfx) for graphics, input, font and audio
  - Box2D Library for Physics
  - Tinyxml library for data-driven approach
  - devkitPro for building the actual 3DS files.


# Building and running

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

If it returns straight to the launcher or crashes, open `sdmc:/3ds/OpenClaw/log.txt` (rewritten
on every launch) - it contains the game's own error messages and usually says what is missing.

## Controls
| 3DS | Action |
|---|---|
| D-pad / Circle Pad | Move, climb, crouch |
| A or L | Jump (A also confirms in menus) |
| B or R | Sword attack |
| X | Fire pistol / magic / dynamite |
| Y | Change weapon |
| START | Pause menu |

## What was changed
- libwap: the REZ directory tree is now parsed through a 64 KB block cache (`RezBlockBuf`). The original
  code did ~50k tiny seeks/reads, which takes minutes on the 3DS SD card.
- Audio: music is OFF by default on 3DS (MIDI through SDL_mixer is too heavy); positional audio
  (`Mix_SetPosition` / `Mix_SetDistance`) is skipped, like the existing Emscripten branches.
- Input: `Platform3DS_PumpInput()` turns buttons into SDL key events.
- Config: Old 3DS uses `config.xml`; New 3DS uses `config_new3ds.xml` if present.
- SDL2_gfx (physics debug drawing) is stubbed out.
- Display: 400x240 window at scale 0.5.
 
# In-game images
  -  **TODO** - add screenshots

**DISCLAIMER**

While Claude Code was used to help build the 3DS version, the output was thoroughly moderated, checked, and supervised. The original OpenClaw project contains NO traces of AI (it was made 9 years ago).

I completely understand the backlash against AI. Personally, I am also critical of its impact, but as a solo developer working on a free-time passion project, it has become an hard-to-avoid tool for optimization. That said, I am not trying to excuse my choice: if you hate AI, you are completely free to skip this project.
