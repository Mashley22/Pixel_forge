#include <catch2/catch_test_macros.hpp>

import PixelForge.core.require;

namespace pf {
  TEST_CASE( "basic", "[core][assert]") {
    REQUIRE_NOTHROW(require(true));
  }
}
