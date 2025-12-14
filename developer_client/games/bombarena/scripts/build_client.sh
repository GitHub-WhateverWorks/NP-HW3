#!/usr/bin/env bash
set -e

echo "[BUILD] Building game clients"

cd "$(dirname "$0")/.."

make clean
make 

echo "[BUILD] Client build complete"
