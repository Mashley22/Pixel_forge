# Conventions

- Macro prefixes: `PIXELFORGE_` for macros controlling compilation, `PF_` for all other macros; `T_` for template parameters, `_c` suffix for concepts.
- Assertions use the internal `pf::require` / `PF_REQUIRE` system, not `assert`: `require` throws on failure in test builds (`PIXELFORGE_REQUIRE_THROWS_ON_FAILURE`) and degrades gracefully in release builds.
- Tests compile with `-Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow ...` — test and lib code must be warning-clean or the build fails.

