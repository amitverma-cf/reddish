#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$ROOT_DIR/build/profile"}
RESULT_DIR=${RESULT_DIR:-"$ROOT_DIR/local/benchmarks/profile-$(date +%Y%m%d-%H%M%S)"}
[[ ! -e "$RESULT_DIR/manifest.txt" ]] || { echo "Refusing to overwrite $RESULT_DIR" >&2; exit 1; }
mkdir -p "$RESULT_DIR"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=OFF -DREDDISH_BUILD_PROFILES=ON
cmake --build "$BUILD_DIR" --parallel "${BUILD_JOBS:-4}"
{
  git -C "$ROOT_DIR" rev-parse HEAD
  git -C "$ROOT_DIR" diff --binary | sha256sum
  uname -a
  c++ --version | head -1
} > "$RESULT_DIR/manifest.txt"
find "$ROOT_DIR/src" -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 | sort -z | xargs -0 sha256sum > "$RESULT_DIR/source-files.sha256"
sha256sum "$BUILD_DIR/output/reddish_profile" > "$RESULT_DIR/binary.sha256"
for shape in reads writes mixed queue; do
  "$BUILD_DIR/output/reddish_profile" "$shape" "${ITERATIONS:-1000000}" > "$RESULT_DIR/$shape.csv"
done
if command -v valgrind >/dev/null; then
  valgrind --tool=callgrind --instr-atstart=no --callgrind-out-file="$RESULT_DIR/callgrind.out" \
    "$BUILD_DIR/output/reddish_profile" mixed "${PROFILE_ITERATIONS:-100000}" > "$RESULT_DIR/callgrind-run.csv" 2> "$RESULT_DIR/callgrind.log"
  callgrind_annotate --inclusive=yes --auto=no --threshold=100 "$RESULT_DIR/callgrind.out" > "$RESULT_DIR/callgrind.txt"
else
  echo 'Install valgrind for instruction-level attribution.' >&2
fi
echo "Profiles: $RESULT_DIR"
