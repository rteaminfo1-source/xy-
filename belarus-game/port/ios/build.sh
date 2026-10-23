#!/bin/sh
# Сборка приложения для iPhone и iPad (нужен Mac с Xcode; в репозитории — GitHub Actions, .github/workflows/ios.yml).
#   port/ios/build.sh            -> build/ios/BelarusTech.ipa (для установки через SideStore / AltStore)
#   port/ios/build.sh simulator  -> build/ios-sim/BelarusTech.app (для снимков экрана в симуляторе)
# Код игры — тот же, что под Windows (src/*.c), окно и шрифты — port/ios/*.m.
set -e
cd "$(dirname "$0")/../.."
KIND=${1:-device}
if [ "$KIND" = simulator ]; then
    SDK=iphonesimulator; TARGET=arm64-apple-ios14.0-simulator; PLATFORM=iPhoneSimulator; OUT=build/ios-sim
else
    SDK=iphoneos; TARGET=arm64-apple-ios14.0; PLATFORM=iPhoneOS; OUT=build/ios
fi
SYSROOT=$(xcrun --sdk $SDK --show-sdk-path)
CC="xcrun --sdk $SDK clang -target $TARGET -isysroot $SYSROOT"
APP=$OUT/Payload/BelarusTech.app
OBJ=$OUT/obj
rm -rf "$OUT"
mkdir -p "$APP" "$OBJ"
FLAGS="-O2 -Isrc -Wall -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unused-function"
for f in src/*.c; do
    $CC -std=c11 $FLAGS -c "$f" -o "$OBJ/$(basename "$f" .c).o" &
    while [ "$(jobs -r | wc -l)" -ge 6 ]; do sleep 0.2; done
done
wait
for f in port/ios/*.m; do
    $CC -fobjc-arc $FLAGS -c "$f" -o "$OBJ/$(basename "$f" .m).o"
done
$CC "$OBJ"/*.o -o "$APP/BelarusTech" \
    -framework UIKit -framework Foundation -framework CoreGraphics -framework CoreText -framework QuartzCore -lm
# ресурсы: рендеры Blender (QOI) и таблица номеров, значки
cp res/*.qoi res/assets.rc "$APP/"
cp port/ios/icons/*.png "$APP/"
BUILD=${BUILD_NUMBER:-1}
PL_LC=$(echo $PLATFORM | tr 'A-Z' 'a-z')
sed -e "s/@BUILD@/$BUILD/" -e "s/@PLATFORM@/$PLATFORM/" -e "s/@PLATFORM_LC@/$PL_LC/" port/ios/Info.plist > "$APP/Info.plist"
plutil -lint "$APP/Info.plist"
printf 'APPL????' > "$APP/PkgInfo"
# подпись «для себя»: SideStore и AltStore переподпишут приложение Apple ID владельца телефона
codesign --force --sign - --timestamp=none "$APP"
if [ "$KIND" != simulator ]; then
    (cd "$OUT" && rm -f BelarusTech.ipa && zip -qr -X BelarusTech.ipa Payload)
    ls -la "$OUT/BelarusTech.ipa"
fi
echo "Готово: $OUT"
