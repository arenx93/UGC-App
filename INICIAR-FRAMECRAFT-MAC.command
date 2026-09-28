#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"

if ! command -v node >/dev/null 2>&1; then
  open "https://nodejs.org/"
  echo "Instala Node.js 24, vuelve a abrir este archivo y prueba otra vez."
  read -r -p "Pulsa Enter para cerrar..."
  exit 1
fi

NODE_MAJOR="$(node -p "process.versions.node.split('.')[0]")"
if [ "$NODE_MAJOR" != "24" ]; then
  open "https://nodejs.org/"
  echo "Framecraft necesita Node.js 24. Version detectada: $(node --version)"
  read -r -p "Pulsa Enter para cerrar..."
  exit 1
fi

if [ ! -f "node_modules/.framecraft-ready" ]; then
  echo "Instalando dependencias. La primera vez puede tardar varios minutos..."
  npm ci --no-audit --no-fund
  printf 'ready\n' > "node_modules/.framecraft-ready"
fi

npm run setup:local

URL="http://localhost:5173"
(
  for _attempt in $(seq 1 120); do
    if curl --silent --fail --output /dev/null "$URL"; then
      open "$URL"
      exit 0
    fi
    sleep 0.5
  done
) &

echo "Abriendo Framecraft en tu navegador predeterminado..."
echo "Mantén esta ventana abierta mientras utilizas la aplicación."
echo "Para cerrar Framecraft, vuelve aquí y pulsa Control+C."
npm run dev -- --host 127.0.0.1 --strictPort
