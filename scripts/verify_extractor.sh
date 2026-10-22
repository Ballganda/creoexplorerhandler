#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/native/creo_param_extractor/build"

cmake -S "$ROOT/native/creo_param_extractor" -B "$BUILD" -DCMAKE_CXX_COMPILER=g++
cmake --build "$BUILD"

"$BUILD/test_extractor"

echo "---- dump_props old ----"
"$BUILD/dump_props" "$ROOT/testdata/detailstest.prt.2"
echo "---- dump_props new ----"
"$BUILD/dump_props" "$ROOT/testdata/detailstest.prt.5"

expected_new=$'Authors=BRO\nTags=Product Line\nComments=Comments\nTitle=Descrip line 1\nSubject=Descrip line 2'
actual_new="$("$BUILD/dump_props" "$ROOT/testdata/detailstest.prt.5")"
if [[ "$actual_new" != "$expected_new" ]]; then
  echo "Unexpected dump_props output for prt.5:" >&2
  echo "$actual_new" >&2
  exit 1
fi
echo "verify_extractor: OK"
