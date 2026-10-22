#!/bin/sh
# Сборка BelarusTech.exe кросс-компилятором MinGW-w64 (Linux) или MinGW/MSYS2 (Windows).
# Никаких сторонних библиотек: только системные DLL Windows (kernel32, user32, gdi32, msvcrt).
set -e
cd "$(dirname "$0")"
CC=${CC:-x86_64-w64-mingw32-gcc}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
mkdir -p build
# Рендеры Blender (assets/*.png) -> res/*.qoi + src/assets_gen.h + res/assets.rc.
# Без Python используются уже упакованные файлы из репозитория.
if command -v python3 >/dev/null 2>&1; then
    python3 tools/pack_assets.py
elif [ ! -f src/assets_gen.h ] || [ ! -f res/assets.rc ]; then
    echo "Нужен python3 для tools/pack_assets.py" >&2; exit 1
fi
$WINDRES -c 65001 -I res res/app.rc -O coff -o build/app.res
$CC -std=c11 -O2 -s -municode -mwindows -static -D__USE_MINGW_ANSI_STDIO=1 \
    -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter \
    src/*.c build/app.res -o BelarusTech.exe -lgdi32 -luser32 -lkernel32 -lm
echo "Готово: $(pwd)/BelarusTech.exe"
# Админ-версия: те же исходники + панель администратора, встроенные презентация и текст выступления (docs/)
PPTX="docs/Презентация - Судьба Республики Беларусь.pptx"; DOCX="docs/Текст выступления.docx"
if [ -f "$PPTX" ] && [ -f "$DOCX" ]; then
    mkdir -p build/admin
    cp "$PPTX" build/admin/presentation.pptx
    cp "$DOCX" build/admin/speech.docx
    $WINDRES -c 65001 -I res -I build/admin -DADMIN_BUILD res/admin.rc -O coff -o build/admin.res
    $CC -std=c11 -O2 -s -municode -mwindows -static -D__USE_MINGW_ANSI_STDIO=1 -DADMIN_BUILD \
        -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter \
        src/*.c build/admin.res -o BelarusTech_Admin.exe -lgdi32 -luser32 -lkernel32 -lm
    echo "Готово: $(pwd)/BelarusTech_Admin.exe"
fi
