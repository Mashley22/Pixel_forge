import PixelForge.core.math;

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

namespace pf {

namespace math {

TEST_CASE( "isPowerOfTwo", "[core][math]" ) {

  STATIC_REQUIRE(isPowerOfTwo<std::uint64_t>(1));

  for (std::size_t i = 2; i < 63; i++) {
    std::uint64_t testVal = ((std::uint64_t)1 << i);
    REQUIRE(isPowerOfTwo<std::uint64_t>(testVal));
    REQUIRE(!isPowerOfTwo<std::uint64_t>(testVal - 1));
  }
}

}

}
