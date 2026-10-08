#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TOOL_DIR=${1:-"$ROOT_DIR/local/benchmark-tools"}
BUILD_JOBS=${BUILD_JOBS:-2}
for program in git make c++ autoreconf automake libtoolize pkg-config; do
  command -v "$program" >/dev/null || { echo "Missing $program" >&2; exit 1; }
done
mkdir -p "$TOOL_DIR"
TOOL_DIR=$(cd "$TOOL_DIR" && pwd)
mkdir -p "$TOOL_DIR/src" "$TOOL_DIR/bin"
for project in memtier redis valkey; do
  case "$project" in
    memtier) repository=https://github.com/RedisLabs/memtier_benchmark.git; revision=5f634d171b83efca9640c5a87606c47b34d3d330 ;;
    redis) repository=https://github.com/redis/redis.git; revision=3399357e7c17b668289386b8a15a3037bc4527b1 ;;
    valkey) repository=https://github.com/valkey-io/valkey.git; revision=fcd8bc3ee40f5d7841b7d5a8f3cd12252fec14e4 ;;
  esac
  source_dir="$TOOL_DIR/src/$project"
  [[ ! -e "$source_dir" ]] || { echo "Refusing to replace $source_dir" >&2; exit 1; }
  git init --quiet "$source_dir"
  git -C "$source_dir" remote add origin "$repository"
  git -C "$source_dir" fetch --quiet --depth 1 origin "$revision"
  git -C "$source_dir" checkout --quiet --detach FETCH_HEAD
  [[ $(git -C "$source_dir" rev-parse HEAD) == "$revision" ]]
  case "$project" in
    memtier)
      (cd "$source_dir" && autoreconf -ivf && ./configure --prefix="$TOOL_DIR" && make --jobs="$BUILD_JOBS" && make install)
      ;;
    redis|valkey)
      make -C "$source_dir" --jobs="$BUILD_JOBS" "$project-server"
      install -m 755 "$source_dir/src/$project-server" "$TOOL_DIR/bin/$project-server"
      ;;
  esac
  printf '%s %s %s\n' "$project" "$revision" "$repository" >> "$TOOL_DIR/revisions.txt"
done
{
  cat "$TOOL_DIR/revisions.txt"
  "$TOOL_DIR/bin/memtier_benchmark" --version
  "$TOOL_DIR/bin/redis-server" --version
  "$TOOL_DIR/bin/valkey-server" --version
  c++ --version
} > "$TOOL_DIR/manifest.txt"
echo "Tools: $TOOL_DIR/bin"
