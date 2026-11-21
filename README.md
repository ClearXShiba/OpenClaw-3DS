# OpenClaw-3DS (Forked from OpenClaw by pjasicek, see https://github.com/pjasicek/OpenClaw) - Captain Claw (1997) reimplementation ported to the 3DS family of systems.

- This project is a C++ reimplementation port (+ some help from Claude Code, more info at the end) of the original OpenClaw to both New and Old 3DS.
- Whole codebase was written from scratch by pjasicek (https://github.com/pjasicek).
- Game uses assets from original game archive (CLAW.REZ)

***This project serves purely as a proof-of-concept, and in no way I'll try to gain money off of it. The project also has NO files from the original game, and it shouldn't serve as an illegal way to play it.***


# Build status

### 3DS:

The third release (0.0.3) finally brought to play some optimization (the sprites are now drawn on the 3DS GPU instead of by the CPU), which means that the game finally reaches 60 fps on a New 3DS, with some minor lags (52-60 fps), but still not so bad.

The second release (0.0.2) manages to get some decent performance (27-30 FPS in-game, 48-49 in menus), and managed to load "La Roca" without modifying anything.

The first ever build (0.0.1), works, but gives VERY LOW performance (3-8 FPS). If you try to boot into "La Roca" as is, the game either crashes or boots you back to The Homebrew Loader. You need to modify the "config_new3ds.xml" in the game's folder for it to actually boot into the level. (ResourceCacheSize).

Proof:

<img width="1084" height="945" alt="screen1" src="https://github.com/user-attachments/assets/38e6fb83-ad86-4f2f-b385-19dacf869fc4" />

Proof 2:

<img width="1033" height="945" alt="screen2" src="https://github.com/user-attachments/assets/afec1b9d-4ba7-41c2-b27e-fb0a3855918b" />


# Tech

  - SDL2 Libraries (SDL2, SDL_Image, SDL_TTF, SDL_Mixer, SDL2_Gfx) for window, input, font and audio
  - citro2d / citro3d (part of devkitPro's `3ds-dev`) for drawing the sprites on the 3DS GPU
  - Box2D Library for Physics
  - Tinyxml library for data-driven approach
  - devkitPro for building the actual 3DS files.


# Building and running

You can find the pre-compiled '.3dsx'  in Releases, configuration files ("config.xml" and "config_new3ds.xml") can be found in the '3ds' folder in the main branch of the repository.

For game-related files ("ASSETS.ZIP", "CLAW.REZ", "console02.tga", "clacon.ttf", "MENU.xml", "SAVES.xml") MUST be taken from your own copy of the game.

If you'd like to compile the application yourself, the instructions are listed down below:

## 1. Install the toolchain (on your PC)
Install devkitPro, then:

    sudo dkp-pacman -S 3ds-dev 3ds-sdl 3ds-sdl_mixer 3ds-sdl_image 3ds-sdl_ttf
    # Windows: open the "devkitPro MSYS2" shell and use "pacman" (not "dkp-pacman"), same packages.
    export DEVKITPRO=/opt/devkitpro
    export DEVKITARM=$DEVKITPRO/devkitARM

(`3ds-dev` already includes citro2d and citro3d, so there is nothing extra to install for the GPU renderer.)

## 2. Apply the patch + add the folder
The easy way, from the devkitPro MSYS2 shell (it finds your source folder, undoes the old patch, applies the
new one, copies the `3ds/` folder and runs `make`):

    bash 3ds/update_and_build.sh
    bash 3ds/update_and_build.sh /c/dev/OpenClaw     # if it picks the wrong folder

Or by hand, from the root of your OpenClaw checkout (same level as `OpenClaw/`, `Box2D/`, `libwap/`):

    git apply 3ds/openclaw-3ds.patch      # edits inside OpenClaw/ (audio, config, input hook, textures)

(The `3ds/` folder from the zip already sits at the repo root.) The patch is applied on top of the original
OpenClaw commit; after pulling a new patch, run `git checkout -- OpenClaw Box2D libwap` first, or just use the script.

## 3. Build

    cd 3ds
    make            # -> OpenClaw.3dsx
    make MUSIC=1    # optional: keep the SDL_mixer MIDI music path (heavy, likely too slow)

The app icon is `3ds/icon.png` (48x48). Replace it with your own and run `make clean && make`.

## 4. Put files on the SD card
Create `sdmc:/3ds/OpenClaw/` and copy:

- `OpenClaw.3dsx` -> `sdmc:/3ds/OpenClaw.3dsx` (or inside that folder)
- `config.xml` (Old 3DS, 16 MB resource cache) and `config_new3ds.xml` (New 3DS, 48 MB cache)
- **your own** `CLAW.REZ`
- `ASSETS.ZIP` = zip of the contents of `Build_Release/ASSETS/`
- `console02.tga` and `clacon.ttf` from `Build_Release/`
- `MENU.xml` / `SAVES.XML` from `Build_Release/` if the game complains

Launch from the Homebrew Launcher.

Optional switch files (empty files in `sdmc:/3ds/OpenClaw/`):

- `software_render.txt` - use the old software renderer instead of the GPU one (slower, but a fallback if the GPU path misbehaves)
- `gpu_flip.txt` - swaps the texture row order of the GPU renderer (only needed if the picture appears upside down)

Tip: the game data and the GPU textures share the 3DS "linear" memory. A very large `ResourceCacheSize` leaves less
room for textures, and the renderer then stores new ones in 16 bit (slightly fewer colours). `log.txt` shows this in the
`[gpu] textures ...` lines.

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
- Graphics: sprites are drawn on the 3DS GPU with citro2d (`Gpu3DS.cpp`). The game still calls the SDL render functions;
  the linker redirects them (`-Wl,--wrap=...`). Textures are converted to the GPU's tiled format at load time,
  padded to power-of-two sizes, and halved in size up front (the screen is drawn at 50% anyway). If linear memory runs low,
  new textures are stored in 16 bit. The old SDL software renderer is kept as a fallback (`software_render.txt`).
- Memory: `new` spills into the 3DS linear heap when the normal heap is full (`Alloc3DS.cpp`), because level 1 needs more
  than the ~90 MB malloc window.
- Diagnostics: `log.txt` has one `[prof]` line per second (fps, physics, actors, draw, GPU wait) and `[mem]` / `[gpu]` memory lines.
 
# In-game images
  -  **TODO** - add screenshots

**DISCLAIMER**

While Claude Code was used to help build the 3DS version, the output was thoroughly moderated, checked, and supervised. The original OpenClaw project contains NO traces of AI (it was made 9 years ago).

I completely understand the backlash against AI. Personally, I am also critical of its impact, but as a solo developer working on a free-time passion project, it has become an hard-to-avoid tool for optimization. That said, I am not trying to excuse my choice: if you hate AI, you are completely free to skip this project.