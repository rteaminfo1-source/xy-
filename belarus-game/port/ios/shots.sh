#!/bin/sh
# Снимки экрана в симуляторе iPhone: собирает приложение для симулятора и снимает сцены разработчика.
#   port/ios/shots.sh [СЦЕНА ...]  -> build/ios-shots/*.png
set -e
cd "$(dirname "$0")/../.."
sh port/ios/build.sh simulator
APP=build/ios-sim/Payload/BelarusTech.app
ID=by.gymnasium10.belarustech
mkdir -p build/ios-shots
DEV=$(xcrun simctl list devices available | grep -E "iPhone 1[5-9] Pro \(|iPhone 1[5-9] \(" | head -1 | sed -E 's/.*\(([0-9A-F-]{36})\).*/\1/')
[ -n "$DEV" ] || DEV=$(xcrun simctl list devices available | grep -E "iPhone" | head -1 | sed -E 's/.*\(([0-9A-F-]{36})\).*/\1/')
echo "Симулятор: $DEV"
xcrun simctl boot "$DEV" || true
xcrun simctl bootstatus "$DEV" -b
xcrun simctl install "$DEV" "$APP"
SCENES=${*:-"1 5 94 68 86 81 34"}
for s in $SCENES; do
    xcrun simctl terminate "$DEV" $ID 2>/dev/null || true
    xcrun simctl launch "$DEV" $ID --scene=$s
    sleep 12
    xcrun simctl io "$DEV" screenshot "build/ios-shots/scene$s.png"
done
# обычный запуск: титульный экран, как увидит игрок
xcrun simctl terminate "$DEV" $ID 2>/dev/null || true
xcrun simctl launch "$DEV" $ID
sleep 8
xcrun simctl io "$DEV" screenshot build/ios-shots/start.png
ls -la build/ios-shots
