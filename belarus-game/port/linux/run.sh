#!/bin/sh
# Сборка без Windows на Linux (та же, что для iPhone, но шрифты — FreeType) и кадр сцены в картинку.
#   port/linux/run.sh SCENE OUT.png [W H SCALE [TAPX TAPY]]
set -e
cd "$(dirname "$0")/../.."
mkdir -p build
if [ ! -x build/host_linux ] || [ -n "$(find src port/linux -newer build/host_linux -name '*.[ch]' | head -1)" ]; then
    gcc -std=c11 -O2 -D_GNU_SOURCE -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter -Isrc \
        $(pkg-config --cflags freetype2) src/*.c port/linux/*.c -o build/host_linux $(pkg-config --libs freetype2) -lm
fi
SCENE=$1
OUT=$2
shift 2
build/host_linux "$SCENE" build/frame.ppm "$@"
convert build/frame.ppm "$OUT"
