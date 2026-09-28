#!/bin/bash
# Builds build/Framecraft.app (universal: Apple Silicon + Intel) and a DMG.
# Usage: scripts/build-app.sh            (VERSION=1.2.0 BUILD_NUMBER=7 optional)
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION="${VERSION:-1.0.0}"
BUILD_NUMBER="${BUILD_NUMBER:-1}"
ARCHS="${ARCHS:-arm64 x86_64}"
OUT="build"

ARCH_ARGS=()
for arch in $ARCHS; do ARCH_ARGS+=(--arch "$arch"); done

echo "▸ Compilando ($ARCHS)…"
swift build -c release "${ARCH_ARGS[@]}"
BIN_DIR="$(swift build -c release "${ARCH_ARGS[@]}" --show-bin-path)"

rm -rf "$OUT"
APP="$OUT/Framecraft.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$BIN_DIR/Framecraft" "$APP/Contents/MacOS/Framecraft"
cp -R "$BIN_DIR/Framecraft_FramecraftCore.bundle" "$APP/Contents/Resources/"
if [ -x build-cache/codex ]; then
  # Bundled Codex CLI (scripts/fetch-codex.sh), signed together with the app.
  cp build-cache/codex "$APP/Contents/MacOS/codex"
  echo "▸ Codex CLI incluido"
fi
sed -e "s/__VERSION__/$VERSION/" -e "s/__BUILD__/$BUILD_NUMBER/" Assets/Info.plist > "$APP/Contents/Info.plist"

echo "▸ Ícono…"
ICONSET="$OUT/AppIcon.iconset"
mkdir -p "$ICONSET"
for size in 16 32 128 256 512; do
  sips -z "$size" "$size" Assets/AppIcon.png --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
  double=$((size * 2))
  sips -z "$double" "$double" Assets/AppIcon.png --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/AppIcon.icns"
rm -rf "$ICONSET"

echo "▸ Firma ad-hoc…"
# Replace "-" with a Developer ID identity to sign for distribution (and then notarize).
IDENTITY="${SIGN_IDENTITY:--}"
SIGN_OPTIONS=()
if [ "$IDENTITY" != "-" ]; then
  # Developer ID: hardened runtime + secure timestamp, required for notarization.
  SIGN_OPTIONS=(--options runtime --timestamp)
  echo "  con identidad: $IDENTITY"
fi
# Sign nested executables first (bundled Codex CLI), then the app itself.
if [ -f "$APP/Contents/MacOS/codex" ]; then
  codesign --force ${SIGN_OPTIONS[@]+"${SIGN_OPTIONS[@]}"} --sign "$IDENTITY" "$APP/Contents/MacOS/codex"
fi
codesign --force ${SIGN_OPTIONS[@]+"${SIGN_OPTIONS[@]}"} --sign "$IDENTITY" "$APP"
codesign --verify --deep --strict "$APP"

echo "▸ DMG…"
DMG_ROOT="$OUT/dmg"
mkdir -p "$DMG_ROOT"
cp -R "$APP" "$DMG_ROOT/"
ln -s /Applications "$DMG_ROOT/Applications"
hdiutil create -volname "Framecraft" -srcfolder "$DMG_ROOT" -ov -format UDZO "$OUT/Framecraft-$VERSION-macOS.dmg" >/dev/null
rm -rf "$DMG_ROOT"
if [ "$IDENTITY" != "-" ]; then
  codesign --force --timestamp --sign "$IDENTITY" "$OUT/Framecraft-$VERSION-macOS.dmg"
fi

echo "✓ $APP"
echo "✓ $OUT/Framecraft-$VERSION-macOS.dmg"
