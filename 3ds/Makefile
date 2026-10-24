#---------------------------------------------------------------------------------
# OpenClaw (Captain Claw 1997 reimplementation) - Nintendo 3DS build
#
# Requirements (devkitPro pacman):
#   sudo dkp-pacman -S 3ds-dev 3ds-sdl 3ds-sdl_mixer 3ds-sdl_image 3ds-sdl_ttf
#
# Usage:   cd 3ds && make            -> OpenClaw.3dsx  (copy to sdmc:/3ds/)
#          make MUSIC=1              -> keep the SDL_mixer MIDI music path (heavy, experimental)
#          make clean
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET          := OpenClaw
BUILD           := build
ROOT            := $(abspath $(CURDIR)/..)
APP_TITLE       := OpenClaw
APP_DESCRIPTION := Captain Claw (1997) reimplementation
APP_AUTHOR      := OpenClaw 3DS port
ICON            := $(DEVKITPRO)/libctru/default_icon.png

#---------------------------------------------------------------------------------
# Sources
#   - OpenClaw/            game + engine (all .cpp)
#   - libwap/              REZ/PID/WWD/... file readers
#   - ThirdParty/Tinyxml   XML parser
#   - Box2D/Box2D          physics
# Excluded:
#   - OpenClaw/Engine/Resource/Miniz.cpp : duplicate of libwap/Miniz.cpp (same code)
#   - MidiProc, ClawLauncher             : Windows only
#---------------------------------------------------------------------------------
CPP_SRCS := $(filter-out $(ROOT)/OpenClaw/Engine/Resource/Miniz.cpp, \
                $(shell find $(ROOT)/OpenClaw -name '*.cpp')) \
            $(wildcard $(ROOT)/libwap/*.cpp) \
            $(wildcard $(ROOT)/ThirdParty/Tinyxml/*.cpp) \
            $(shell find $(ROOT)/Box2D/Box2D -name '*.cpp') \
            $(ROOT)/3ds/Platform3DS.cpp

OBJS := $(patsubst $(ROOT)/%.cpp,$(BUILD)/%.o,$(CPP_SRCS))
DEPS := $(OBJS:.o=.d)

#---------------------------------------------------------------------------------
# Flags
#---------------------------------------------------------------------------------
PKGCONF := $(DEVKITPRO)/portlibs/3ds/bin/arm-none-eabi-pkg-config
SDL_PKGS := sdl2 SDL2_mixer SDL2_image SDL2_ttf

ifeq ($(strip $(shell $(PKGCONF) --exists $(SDL_PKGS) && echo yes)),)
$(error 3DS SDL2 libraries not found by $(PKGCONF). Install them with: dkp-pacman -S 3ds-sdl 3ds-sdl_mixer 3ds-sdl_image 3ds-sdl_ttf   (list names with: dkp-pacman -Ss sdl))
endif

ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

# Order matters: devkitPro/portlibs headers first, so <SDL2/SDL.h> resolves to the 3DS SDL2
# and NOT to the bundled Windows copy in ThirdParty/SDL2.
INCLUDE  := $(shell $(PKGCONF) --cflags $(SDL_PKGS)) -I$(PORTLIBS)/include -I$(PORTLIBS)/include/SDL2 \
            -I$(CTRULIB)/include \
            -I$(ROOT)/3ds -I$(ROOT)/Box2D -I$(ROOT)/libwap -I$(ROOT)/ThirdParty \
            -I$(ROOT)/ThirdParty/Tinyxml

DEFINES  := -D__3DS__ -DSDL_MAIN_HANDLED -DTIXML_USE_STL
ifeq ($(MUSIC),1)
DEFINES  += -DOPENCLAW_3DS_MUSIC
endif

CFLAGS   := -g -Wall -O2 -mword-relocations -ffunction-sections -fomit-frame-pointer \
            $(ARCH) $(DEFINES) $(INCLUDE)
CXXFLAGS := $(CFLAGS) -std=gnu++14 -Wno-reorder -Wno-sign-compare -Wno-unused-variable

LDFLAGS  := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(BUILD)/$(TARGET).map -L$(CTRULIB)/lib -L$(PORTLIBS)/lib

LIBS     := $(shell $(PKGCONF) --static --libs $(SDL_PKGS)) -lcitro3d -lctru -lm

#---------------------------------------------------------------------------------
.PHONY: all clean

all: $(TARGET).3dsx

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
	@echo "  3DSXTOOL $@"
	@$(DEVKITPRO)/tools/bin/3dsxtool $< $@ --smdh=$(TARGET).smdh

$(TARGET).smdh:
	@echo "  SMDHTOOL $@"
	@$(DEVKITPRO)/tools/bin/smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $(ICON) $@

$(TARGET).elf: $(OBJS)
	@echo "  LD       $@"
	@$(CXX) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(BUILD)/%.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	@echo "  CXX      $(notdir $<)"
	@$(CXX) -MMD -MP $(CXXFLAGS) -c $< -o $@

clean:
	@echo "  clean"
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).3dsx $(TARGET).smdh

-include $(DEPS)
