#!/bin/sh
# Сборка BelarusTech.exe кросс-компилятором MinGW-w64 (Linux) или MinGW/MSYS2 (Windows).
# Никаких сторонних библиотек: только системные DLL Windows (kernel32, user32, gdi32, msvcrt).
set -e
cd "$(dirname "$0")"
CC=${CC:-x86_64-w64-mingw32-gcc}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
mkdir -p build
$WINDRES -c 65001 -I res res/app.rc -O coff -o build/app.res
$CC -std=c11 -O2 -s -municode -mwindows -static -D__USE_MINGW_ANSI_STDIO=1 \
    -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter \
    src/*.c build/app.res -o BelarusTech.exe -lgdi32 -luser32 -lkernel32 -lm
echo "Готово: $(pwd)/BelarusTech.exe"
