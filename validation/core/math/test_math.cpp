#include <PixelForgeValidationHelpers/helpers.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>

import PixelForge.core;

namespace pf {

namespace math {

PF_TEST_CASE("is_power_of_two", "[core][math]") {

  STATIC_REQUIRE(is_power_of_two<std::uint64_t>(1));

  for (std::size_t i = 2; i < 63; i++) {
    std::uint64_t testVal = (static_cast<std::uint64_t>(1) << i);
    REQUIRE(is_power_of_two<std::uint64_t>(testVal));
    REQUIRE(!is_power_of_two<std::uint64_t>(testVal - 1));
  }
}

}

}
