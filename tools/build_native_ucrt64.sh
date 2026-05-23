#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

cmake -S . -B build -G Ninja
cmake --build build

echo
echo "Native tools built in build/native/"
echo "Example:"
echo "  ./build/native/pluto_scan_session.exe --config configs/2m.conf --dry-run"
