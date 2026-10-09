#!/usr/bin/env bash

set -e

cd "$(dirname "$0")"

echo "=========================================="
echo "   REPRODUCTOR MP3 CONCURRENTE - SO2026B"
echo "=========================================="
echo

if ! command -v pkg-config >/dev/null 2>&1; then
    echo "[ERROR] pkg-config no esta instalado."
    exit 1
fi

if ! pkg-config --exists libmpg123; then
    echo "[ERROR] Falta libmpg123-dev."
    echo "Instale con:"
    echo "sudo apt install libmpg123-dev mpg123"
    exit 1
fi

if ! pkg-config --exists ao; then
    echo "[ERROR] Falta libao-dev."
    echo "Instale con:"
    echo "sudo apt install libao-dev libao4"
    exit 1
fi

make

echo
echo "Iniciando reproductor..."
echo

./audio_player