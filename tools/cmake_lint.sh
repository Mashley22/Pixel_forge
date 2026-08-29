#!/usr/bin/env bash
set -euo pipefail

# Run cmakelint on all CMakeLists.txt files (excluding extern/)
# Requires: pip install cmakelint

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

find . -name CMakeLists.txt -not -path './extern/*' -exec cmakelint --config=.cmake-lint {} \;
