#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

case "${1:-}" in
	--clean) rm -rf build ;;
	""|--build) ;;
	*) echo "Usage : $0 [--build|--clean]"; exit 1 ;;
esac

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"

[ "${1:-}" = "--build" ] && exit 0
cd build && exec ./DriftGame
