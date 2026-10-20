#!/bin/sh
# Тесты на Linux (без Windows): симуляция партий и сохранений, 3D-рельеф, конвейер картинок,
# а также tests/*_test.c модулей движка. Запуск из корня belarus-game: sh tests/run.sh
set -e
cd "$(dirname "$0")/.."
OUT=${TMPDIR:-/tmp}/belarus-tests-$$
mkdir -p "$OUT"
# переносимые модули движка (без windows.h)
ENGINE="src/engine.c src/war.c src/front.c src/citynet.c src/data_*.c"
CF="-std=c11 -O2 -Wall -Wno-missing-field-initializers -Isrc"
gcc $CF tests/sim.c $ENGINE -lm -o "$OUT/sim" && "$OUT/sim"
# 3D-рельеф: рендер нескольких видов в $OUT (нужны res/terrain*.qoi)
gcc $CF tests/terrain_test.c src/terrain3d.c src/image.c -lm -o "$OUT/terrain" && "$OUT/terrain" "$OUT" | tail -3
# картинки: синтетические данные pack_assets.py (нужен PIL)
if python3 -c "import PIL" 2>/dev/null; then
    python3 tools/pack_assets.py --testdata "$OUT/qoi" >/dev/null
    gcc -std=c11 -O1 -I"$OUT/qoi" tests/image_test.c -o "$OUT/image" -lm && "$OUT/image" "$OUT/qoi" | tail -2
fi
# тесты модулей: tests/<модуль>_test.c с движком
for t in tests/*_test.c; do
    case "$t" in tests/terrain_test.c|tests/image_test.c) continue ;; esac
    gcc $CF "$t" $ENGINE -lm -o "$OUT/t" && "$OUT/t"
done
rm -rf "$OUT"
echo "Все тесты пройдены"
