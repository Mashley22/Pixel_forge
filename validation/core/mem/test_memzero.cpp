import PixelForge.core.mem.memzero;

import PixelForge.core.require;

#include <catch2/catch_test_macros.hpp>

#define NON_ZERO_VALUE 8
#define TEST_ARR_LEN 1000

static_assert(NON_ZERO_VALUE != 0);

namespace pf {

namespace mem {

namespace {

struct NotZeroByte {
  std::byte data{8};
};

}

TEST_CASE( "memzero", "[core][mem]" ) {

  SECTION( "basic usage" ) {
    NotZeroByte arr[TEST_ARR_LEN] = {};

    memzero(arr, TEST_ARR_LEN * sizeof(NotZeroByte));

    for (std::size_t i = 0; i < TEST_ARR_LEN; i++) {
      REQUIRE(arr[i].data == std::byte{0});
    }
  } 

  SECTION( "no null", "[core][mem]" ) {
    REQUIRE_THROWS_AS(memzero(reinterpret_cast<void*>(NON_ZERO_VALUE), 0), RequireFail);
    REQUIRE_THROWS_AS(memzero(reinterpret_cast<void*>(0), NON_ZERO_VALUE), RequireFail);
  }

}

TEST_CASE( "memzero_explicit_basic", "[core][mem]" ) {

  SECTION( "basic usage" ) {
    NotZeroByte arr[TEST_ARR_LEN] = {};

    memzero_explicit(arr, TEST_ARR_LEN * sizeof(NotZeroByte));

    for (std::size_t i = 0; i < TEST_ARR_LEN; i++) {
      REQUIRE(arr[i].data == std::byte{0});
    }
  } 

  SECTION( "no null", "[core][mem]" ) {
    REQUIRE_THROWS_AS(memzero_explicit(reinterpret_cast<void*>(NON_ZERO_VALUE), 0), RequireFail);
    REQUIRE_THROWS_AS(memzero_explicit(reinterpret_cast<void*>(0), NON_ZERO_VALUE), RequireFail);
  }

}


}

}
