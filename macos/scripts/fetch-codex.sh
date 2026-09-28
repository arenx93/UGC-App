#!/bin/bash
# Downloads the official Codex CLI for macOS (Apple Silicon + Intel) and
# produces a universal binary at build-cache/codex, bundled into Framecraft.app so
# users only need to sign in with ChatGPT. Source: github.com/openai/codex releases,
# with the @openai/codex npm package as a fallback.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="build-cache/codex-download"
rm -rf "$OUT"; mkdir -p "$OUT" build-cache

AUTH=()
if [ -n "${GITHUB_TOKEN:-}" ]; then AUTH=(-H "Authorization: Bearer $GITHUB_TOKEN"); fi

fetch_release() {
  local triple="$1"
  local json url
  json="$(curl -fsSL ${AUTH[@]+"${AUTH[@]}"} https://api.github.com/repos/openai/codex/releases/latest)" || return 1
  url="$(printf '%s' "$json" | python3 -c "
import sys, json
assets = json.load(sys.stdin)['assets']
names = ['codex-$triple.tar.gz', 'codex-$triple.zst']
for want in names:
    for a in assets:
        if a['name'] == want:
            print(a['browser_download_url']); sys.exit()
")" || return 1
  [ -n "$url" ] || return 1
  curl -fsSL "$url" -o "$OUT/$triple.tar.gz"
  mkdir -p "$OUT/$triple"
  tar -xzf "$OUT/$triple.tar.gz" -C "$OUT/$triple"
  local bin
  bin="$(find "$OUT/$triple" -type f -name 'codex*' ! -name '*.tar.gz' | head -1)"
  [ -n "$bin" ] || return 1
  cp "$bin" "$OUT/codex-$triple"
}

fetch_npm() {
  local triple="$1"
  if [ ! -d "$OUT/npm" ]; then
    mkdir -p "$OUT/npm"
    (cd "$OUT/npm" && npm pack @openai/codex@latest >/dev/null 2>&1 && tar -xzf openai-codex-*.tgz)
  fi
  local bin
  bin="$(find "$OUT/npm" -path "*$triple*" -type f -name codex | head -1)"
  [ -n "$bin" ] || return 1
  cp "$bin" "$OUT/codex-$triple"
}

for triple in aarch64-apple-darwin x86_64-apple-darwin; do
  fetch_release "$triple" || fetch_npm "$triple" || { echo "No se pudo descargar Codex para $triple"; exit 1; }
done

lipo -create "$OUT/codex-aarch64-apple-darwin" "$OUT/codex-x86_64-apple-darwin" -output build-cache/codex
chmod +x build-cache/codex
rm -rf "$OUT"
echo "✓ build-cache/codex ($(build-cache/codex --version 2>/dev/null || echo 'versión desconocida'))"
