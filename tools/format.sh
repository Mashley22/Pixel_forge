#!/usr/bin/env bash
set -euo pipefail

# Format (or check) all tracked C++ sources with clang-format.
#
# Usage:
#   scripts/format.sh                 check all files (--dry-run --Werror)
#   scripts/format.sh --fix           format in place
#   scripts/format.sh --fix <file>... format only the given files
#   scripts/format.sh --check <file>...  check only the given files
#   scripts/format.sh --help

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

mode=check
files=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --fix | -i)
      mode=fix
      ;;
    --check | -c)
      mode=check
      ;;
    --help | -h)
      sed -n '4,11p' "${BASH_SOURCE[0]}"
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

if [[ ${#files[@]} -eq 0 ]]; then
  mapfile -t files < <(git ls-files '*.cpp' '*.cppm' '*.hpp')
fi

command -v clang-format >/dev/null || {
  echo "error: clang-format not found" >&2
  exit 1
}

case "$mode" in
  fix)
    clang-format -i --style=file "${files[@]}"
    ;;
  check)
    clang-format --dry-run --Werror --style=file "${files[@]}"
    ;;
esac
