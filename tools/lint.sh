#!/usr/bin/env bash
set -euo pipefail

# Lint all tracked C++ sources with clang-tidy, excluding system, standard
# library, and extern/ dependency code.
#
# Usage:
#   scripts/lint.sh                          lint all files (41 TUs, parallel)
#   scripts/lint.sh --fix                    auto-apply safe clang-tidy fixes
#   scripts/lint.sh --verbose                show per-file progress (default is quiet)
#   scripts/lint.sh --jobs N                 override $(nproc)
#   scripts/lint.sh --preset NAME            compile DB under build/<NAME>
#   scripts/lint.sh <file>...                lint only the given files
#   scripts/lint.sh --help
#
# A compile database is required; run `cmake --preset debug_validation` first.

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

preset=debug_validation
jobs="$(nproc)"
quiet=true
fix=false
files=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --fix | -f)
      fix=true
      ;;
    --quiet | -q)
      quiet=true
      ;;
    --verbose | -v)
      quiet=false
      ;;
    --jobs | -j)
      jobs="$2"
      shift
      ;;
    --preset | -p)
      preset="$2"
      shift
      ;;
    --help | -h)
      sed -n '4,14p' "${BASH_SOURCE[0]}"
      exit 0
      ;;
    --)
      shift
      files+=("$@")
      break
      ;;
    -*)
      echo "unknown option: $1" >&2
      exit 1
      ;;
    *)
      files+=("$1")
      ;;
  esac
  shift
done

for tool in clang-tidy run-clang-tidy; do
  command -v "$tool" >/dev/null || {
    echo "error: $tool not found" >&2
    exit 1
  }
done

compile_db="build/$preset/compile_commands.json"
if [[ ! -f "$compile_db" ]]; then
  echo "error: $compile_db not found; run: cmake --preset $preset" >&2
  exit 1
fi

extra=()
$quiet && extra+=(-quiet)
$fix && extra+=(-fix)

# run-clang-tidy always prints its banner and per-file progress lines even with
# -quiet; strip them (only in quiet mode) so diagnostics reach the terminal.
noise_filter='^(Running clang-tidy in |\[ *[0-9]+/[0-9]+\]\[|\[[0-9]+/[0-9]+\] \([0-9]+/[0-9]+\) Processing file )'

if [[ ${#files[@]} -gt 0 ]]; then
  cmd=(clang-tidy -p "build/$preset" "${extra[@]}" "${files[@]}")
else
  mapfile -t files < <(git ls-files '*.cpp' '*.cppm' '*.hpp')
  cmd=(run-clang-tidy -p "build/$preset" -j "$jobs" "${extra[@]}" "${files[@]}")
fi

if $quiet; then
  set +e
  "${cmd[@]}" 2>&1 | grep -vE "$noise_filter"
  status=${PIPESTATUS[0]}
  set -e
  exit "$status"
fi

"${cmd[@]}"
