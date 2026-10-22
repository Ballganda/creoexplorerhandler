#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/native/creo_param_extractor/build"

cmake -S "$ROOT/native/creo_param_extractor" -B "$BUILD" -DCMAKE_CXX_COMPILER=g++
cmake --build "$BUILD" --target stress_hotpath test_extractor

"$BUILD/test_extractor"
"$BUILD/stress_hotpath" "$ROOT/testdata/detailstest.prt.5" 1000 5000
