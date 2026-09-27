#!/usr/bin/env bash
# clang-tidy over the project's own sources, failing on any finding.
#
#   ci/tidy.sh [build-dir]        (default: build/dev; must hold compile_commands.json)
#
# CLANG_TIDY picks the binary (default: clang-tidy). Tests and fetched
# dependencies are left out: doctest's macros are not ours to restyle.

set -euo pipefail

BUILD="${1:-build/dev}"
TIDY="${CLANG_TIDY:-clang-tidy}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if [[ ! -f "$BUILD/compile_commands.json" ]]; then
    echo "no $BUILD/compile_commands.json — configure a preset first" >&2
    exit 2
fi

files=$(cd "$ROOT" && git ls-files 'core/src/*.cpp' 'storage/src/*.cpp' 'tools/*.cpp' 'app/src/*.cpp')
jobs=$( (nproc || sysctl -n hw.ncpu) 2>/dev/null)

# A Homebrew clang-tidy does not know where Apple's SDK headers live.
extra=()
if command -v xcrun >/dev/null; then extra=(--extra-arg="-isysroot$(xcrun --show-sdk-path)"); fi

cd "$ROOT"
# shellcheck disable=SC2086
printf '%s\n' $files | xargs -P "$jobs" -n 1 "$TIDY" -p "$BUILD" --quiet --warnings-as-errors='*' "${extra[@]}"
