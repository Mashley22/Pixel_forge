# AGENTS.md

## Build & test
- C++23 named-module project. CMake ≥ 3.28, Ninja, clang by default (see `CMakePresets.json`).
- **Tests are only built by the `*_validation` / `perf_val` presets** (`PIXELFORGE_TEST=ON`). Plain `base`/`debug_base` builds skip `validation/` entirely — an agent editing tests must configure a validation preset.
- Typical workflow:
  - `cmake --preset debug_validation`
  - `cmake --build --preset debug_validation` (or `ninja -C build/debug_validation`)
  - `ctest --test-dir build/debug_validation`
- Each Catch2 `TEST_CASE` is its own ctest entry (`catch_discover_tests`). To run one suite/binary:
  - `ctest --test-dir build/debug_validation -R strcpy`
  - or run directly: `build/debug_validation/validation/core/utils/test_strcpy`
- `perf_val` presets `FATAL_ERROR` unless `PIXELFORGE_AGGRESSIVE_OPTIMISATIONS` is on (it is, in the release presets) and use Benchpp.
- Submodules: run `git submodule update --init` (extern/catch2; extern/benchpp is a personal fork).

## Module architecture
- Public APIs are C++ modules (`.cppm`), not headers. Each lib area (e.g. `libs/core/utils/`) has a partition-interface file (`utils.cppm`) that re-exports submodules via `export import :utils.strcpy;`. `libs/core/core.cppm` re-exports all core partitions (`:require`, `:math`, `:errors`, `:utils`, `:meta`); tests `import PixelForge.core;`.
- To add a core module: create `libs/core/<area>/modules/<name>.cppm` (module `PixelForge.core:<area>.<name>`), re-export it from the area's interface `.cppm`, and register it with `target_cxx_modules_public(PixelForge_core ...)` in that area's `CMakeLists.txt`. Subdirectory order matters: `require` must be added first (see `libs/core/CMakeLists.txt`).
- **Stale trap:** there is no `libs/core/strcpy/` or `validation/core/strcpy/` anymore — strcpy moved into `utils/`. The real `strcpy`/`copy_until` live in `libs/core/utils/modules/strcpy.cppm`; its tests are in `validation/core/utils/test_strcpy.cpp`.
- Headers exist only for macros that modules can't export: `#include <PixelForge/core/macros.hpp>`.

## Conventions & gotchas
- `conventions.txt`: `PIXELFORGE_` prefix for macros controlling compilation, `PF_` for other macros; `T_` prefix for template params; `_c` suffix for concepts.
- Assertions use the internal `pf::require` / `PF_REQUIRE` system, not `assert`. In test builds `PIXELFORGE_REQUIRE_THROWS_ON_FAILURE` is defined on `PixelForge_core`, so `require` throws and `PF_NOEXCEPT` expands to nothing; release builds differ.
- Tests compile with `-Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow ...` (`PEDANTIC_COMPILE_OPTIONS`) — they must be warning-clean or the build fails.

## Lint & format
- `.clang-format` and `.clang-tidy` configs live at the repo root. Style follows the repo's dominant conventions (2-space indent, `AlwaysBreakAfterReturnType`, namespaces not indented).
- Format or lint the whole project via `scripts/format.sh` and `scripts/lint.sh` (see `--help`). Format defaults to check-only (`--dry-run --Werror`); use `--fix` to format in place. Lint runs `run-clang-tidy` over all tracked sources and needs `cmake --preset debug_validation` for the compile DB.
- Format a single file: `clang-format -i <file>`; check only: `clang-format --dry-run --Werror <file>`.
- Run clang-tidy on a single file with the compile database: `clang-tidy -p build/debug_validation <file>`.
- Run clang-tidy on the whole project (excludes `extern/` sources — Catch2 ships its own `.clang-tidy` that would otherwise apply, and system/stdlib headers are skipped by default):
  `run-clang-tidy -p build/debug_validation -quiet -j $(nproc) $(git ls-files '*.cpp' '*.cppm' '*.hpp')`
- `.clang-tidy` gotchas: `HeaderFilterRegex`/`ExcludeHeaderFilterRegex` only apply to non-main files, so passing the file list above is what keeps dependency sources out. The repo dir is `Pixel_forge` (lowercase `f`), so the header regex `'PixelForge'` matches only real headers under `include/PixelForge/` — a broader regex like `^.*/libs/` makes module interfaces re-report their declarations once per consumer TU. Comments must sit *outside* the `Checks: >` folded scalar or they corrupt the check list. `bugprone-throwing-static-initialization` is excluded because Catch2's `TEST_CASE` macro triggers it on every test.
- Known clang-format 22 wart: requires-expressions inside `concept` declarations get over-indented (e.g. `err_policy.cppm`). Wrap such blocks in `// clang-format off` / `// clang-format on` if it bothers you.
