# OpenClaw-3DS (Forked from OpenClaw by pjasicek, see https://github.com/pjasicek/OpenClaw) - Captain Claw (1997) reimplementation ported to the 3DS family of       systems.

 - This project is a C++ reimplementation port (+ some help from Claude Code, more info at the end) of the original OpenClaw to both New and Old 3DS.
 - Whole codebase was written from scratch by pjasicek (https://github.com/pjasicek).
 - Game uses assets from original game archive (CLAW.REZ)

**Level 1 youtube playthrough:**

[![OpenClaw - Level 1](https://img.youtube.com/vi/ikXQNV_aw68/0.jpg)](https://www.youtube.com/watch?v=ikXQNV_aw68)

# Build status

### Windows:

[![Build Status](https://ci.appveyor.com/api/projects/status/github/pjasicek/OpenClaw?svg=true)](https://ci.appveyor.com/project/pjasicek/captainclaw)

### Linux (debian), macOS, Windows, Android, Emscripten:

[![Build Status](https://travis-ci.org/pjasicek/OpenClaw.svg?branch=master)](https://travis-ci.org/pjasicek/OpenClaw)

### Static analysis / Code coverage:

[![Static analysis](https://scan.coverity.com/projects/12422/badge.svg?flat=1)](https://scan.coverity.com/projects/pjasicek-captainclaw)

# Tech

  - SDL2 Libraries (SDL2, SDL_Image, SDL_TTF, SDL_Mixer, SDL2_Gfx) for graphics, input, font and audio
  - Box2D Library for Physics
  - Tinyxml library for data-driven approach
  - devkitPro for building the actual 3DS files.

# Building and running
You will need original `CLAW.REZ` game archive in `Build_Release` directory from original game

Zip all content inside `Build_Release/ASSETS` directory to `ASSETS.ZIP` file.


### Nintendo 3DS (Old and New)

  - TBT

 
# In-game images
  -  **TODO** - add screenshots

**DISCLAIMER**

While Claude Code was used to help build the 3DS version, the output was thoroughly moderated, checked, and supervised. The original OpenClaw project contains NO traces of AI (it was made 9 years ago).

I completely understand the backlash against AI. Personally, I am also critical of its impact, but as a solo developer working on a free-time passion project, it has become an hard-to-avoid tool for optimization. That said, I am not trying to excuse my choice: if you hate AI, you are completely free to skip this project.
