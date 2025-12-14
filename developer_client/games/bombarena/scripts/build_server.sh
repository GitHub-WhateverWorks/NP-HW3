#!/usr/bin/env bash
set -e

echo "[BUILD] Building game server"

cd "$(dirname "$0")/.."

make clean
make game_server

echo "[BUILD] Server build complete"
