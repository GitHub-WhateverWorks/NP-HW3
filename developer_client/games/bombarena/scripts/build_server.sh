#!/usr/bin/env bash
set -e

echo "[BUILD] Building game server"

cd "$(dirname "$0")/.."

make clean
make 

echo "[BUILD] Server build complete"
