# PixelForge

Early-stage (v0.0.1) C++23 engine/library project built on **C++ named modules**: public APIs are modules (`.cppm`), not headers. Code lives in the `pf` namespace, with `pf::mem`, `pf::log`, `pf::adapters`, etc. for subsystems.

## Requirements

- CMake ≥ 3.28 (C++23 modules support)
- Ninja
- A compiler with C++23 named-module support — clang by default (`base` presets), gcc variants also provided
- Submodules fetched with `git submodule update --init` (`extern/catch2`; `extern/benchpp` is a personal fork)

## Building & testing

Tests and performance validations are only built by the dedicated presets — plain builds skip `validation/` and `benchmarks/` entirely.

```sh
# configure + build + test (clang, Debug, Catch2 tests ON)
cmake --preset debug_validation
cmake --build --preset debug_validation
ctest --test-dir build/debug_validation
```

CMake options (`options.cmake`):

- `PIXELFORGE_TEST` — build the unit tests (`validation/`)
- `PIXELFORGE_PERF_VAL` — build the performance benchmarks (`benchmarks/`); implies `PIXELFORGE_TEST`, and is a `FATAL_ERROR` unless aggressive optimisations are on
- `PIXELFORGE_AGGRESSIVE_OPTIMISATIONS` — `-O3` with LTO/IPO when supported

## Lint & format

`.clang-format` and `.clang-tidy` configs live at the repo root; style follows the repo's dominant conventions (2-space indent, `AlwaysBreakAfterReturnType`, namespaces not indented).

```sh
./scripts/format.sh          # check-only (--dry-run --Werror); use --fix to format in place
./scripts/lint.sh            # run-clang-tidy over all tracked sources; needs debug_validation configured
clang-format -i <file>       # format a single file
clang-tidy -p build/debug_validation <file>
```

## License

MPL 2.0 — see [LICENSE](LICENSE).
