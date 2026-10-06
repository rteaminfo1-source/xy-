#!/usr/bin/env bash
# Сборка C++ движка на Linux/macOS (для тестов движка; интерфейс WinForms работает в Windows).
set -e
cd "$(dirname "$0")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
echo "Готово: $(ls build/*hronograf_engine* 2>/dev/null)"
