#!/bin/bash
# One-shot updater + builder for the OpenClaw 3DS port. Run it from the devkitPro MSYS2 shell:
#
#     bash /path/to/the/unzipped/3ds/update_and_build.sh
#     bash /path/to/the/unzipped/3ds/update_and_build.sh /c/dev/OpenClaw     # if auto-detect picks wrong
#
# What it does:
#   1. finds your OpenClaw source folder (the one containing OpenClaw/, Box2D/ and libwap/)
#   2. removes the previously applied patch from the game sources
#   3. copies this 3ds/ folder next to them, applies the new patch
#   4. runs make and tells you where OpenClaw.3dsx ended up

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "New 3DS files are in: $HERE"

if [ -z "$DEVKITARM" ]; then
    echo "DEVKITARM is not set - run this in the devkitPro MSYS2 shell (not plain Git Bash)."
    exit 1
fi

# ---- 1. find the source root ------------------------------------------------------------------
ROOT="$1"
if [ -z "$ROOT" ]; then
    CANDIDATES=$(find /c/dev /c/Users "$HOME" -maxdepth 7 -type d -name libwap 2>/dev/null | while read -r d; do
        p="$(dirname "$d")"
        if [ -d "$p/Box2D" ] && [ -d "$p/OpenClaw" ]; then echo "$p"; fi
    done | sort -u)

    # Prefer a folder that already has a 3ds/ build in it
    ROOT=$(echo "$CANDIDATES" | while read -r p; do
        [ -n "$p" ] && [ -f "$p/3ds/Makefile" ] && echo "$p"
    done | head -1)
    [ -z "$ROOT" ] && ROOT=$(echo "$CANDIDATES" | head -1)

    if [ "$(echo "$CANDIDATES" | grep -c .)" -gt 1 ]; then
        echo "Found several OpenClaw source folders:"
        echo "$CANDIDATES" | sed 's/^/    /'
        echo "Using: $ROOT   (pass another path as the first argument to override)"
    fi
fi

if [ -z "$ROOT" ] || [ ! -d "$ROOT/OpenClaw" ] || [ ! -d "$ROOT/Box2D" ] || [ ! -d "$ROOT/libwap" ]; then
    echo "Could not find your OpenClaw source folder (needs OpenClaw/, Box2D/ and libwap/ inside)."
    echo "Re-run with the path:  bash $0 /c/path/to/OpenClaw"
    exit 1
fi
echo "OpenClaw source folder: $ROOT"
cd "$ROOT" || exit 1

# ---- 2. undo the previously applied patch -----------------------------------------------------
if [ -d .git ]; then
    echo "Resetting game sources with git..."
    git checkout -- OpenClaw Box2D libwap || exit 1
elif [ -f 3ds/openclaw-3ds.patch ] && [ "$ROOT/3ds" != "$HERE" ]; then
    echo "Not a git checkout - reversing the old patch..."
    patch -R -p1 -s < 3ds/openclaw-3ds.patch || echo "WARNING: could not fully reverse the old patch (continuing)"
fi

# ---- 3. copy new files + apply patch ----------------------------------------------------------
if [ "$ROOT/3ds" != "$HERE" ]; then
    mkdir -p "$ROOT/3ds"
    cp -r "$HERE"/. "$ROOT/3ds/" || exit 1
    echo "Copied new 3ds/ files."
fi

echo "Applying patch..."
if [ -d .git ]; then
    git apply --ignore-whitespace 3ds/openclaw-3ds.patch || { echo "PATCH FAILED - send me the lines above."; exit 1; }
else
    patch -p1 < 3ds/openclaw-3ds.patch || { echo "PATCH FAILED - send me the lines above."; exit 1; }
fi

# ---- 4. build ---------------------------------------------------------------------------------
cd "$ROOT/3ds" || exit 1
echo "Building (a few minutes the first time, faster afterwards)..."
make 2>&1 | tee build.log
if [ -f OpenClaw.3dsx ]; then
    echo
    echo "DONE. Copy this file to SD:/3ds/OpenClaw/ :"
    echo "    $(cygpath -w "$ROOT/3ds/OpenClaw.3dsx" 2>/dev/null || echo "$ROOT/3ds/OpenClaw.3dsx")"
else
    echo
    echo "Build did not produce OpenClaw.3dsx - send me: $ROOT/3ds/build.log (or the errors above)."
    exit 1
fi
