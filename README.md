# OpenClaw-3DS (Forked from OpenClaw by pjasicek, see https://github.com/pjasicek/OpenClaw) - Captain Claw (1997) reimplementation ported to the 3DS family of       systems.

 - This project is a C++ reimplementation port (+ some help from Claude Code, more info at the end) of the original OpenClaw to both New and Old 3DS.
 - Whole codebase was written from scratch by pjasicek (https://github.com/pjasicek).
 - Game uses assets from original game archive (CLAW.REZ)

**Level 1 youtube playthrough:**

[![OpenClaw - Level 1](https://img.youtube.com/vi/ikXQNV_aw68/0.jpg)](https://www.youtube.com/watch?v=ikXQNV_aw68)

# Build status

### 3DS:

The game is currently 10% complete; only the menu really works, and if  you try to boot into a level, the game will send you back to The Homebrew Loader.

<img width="1084" height="945" alt="screen1" src="https://github.com/user-attachments/assets/38e6fb83-ad86-4f2f-b385-19dacf869fc4" />

<img width="1033" height="945" alt="screen2" src="https://github.com/user-attachments/assets/afec1b9d-4ba7-41c2-b27e-fb0a3855918b" />


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
