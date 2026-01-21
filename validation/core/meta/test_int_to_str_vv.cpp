import PixelForge.core.meta;

#include <array>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#define TEST_VALS_NUM 3

namespace pf {

namespace meta {

namespace {

constexpr std::array<int, TEST_VALS_NUM> testInts = {
  5, 9, -420
};

constexpr std::array<std::string_view, TEST_VALS_NUM> testStrs = {
  "5", "9", "-420"
};

auto strToIntSuccessful = []() {
  for (std::size_t i = 0; i < TEST_VALS_NUM; i++) {
    if (strToInt<int>(testStrs[i]) != testInts[i]) {
      return false;
    }
  }

  return true;
};

template<
  std::size_t T_idx = testInts.size()
>
[[nodiscard]]
consteval
bool 
intToStrSuccessful(void) {
  if (!intToStrSuccessful<T_idx - 1>()) {
    return false;
  }

  return IntToStr<int, 1>::sv() == "1";//testVals[T_idx].str;
};

template<>
[[nodiscard]]
consteval
bool 
intToStrSuccessful<0>(void) {
  return IntToStr<int, testInts[0]>::sv() == testStrs[0];
}

}

TEST_CASE( "test svToInt", "[core][meta]" ) {
  STATIC_REQUIRE( strToIntSuccessful() == true );
}

TEST_CASE( "test intToSv", "[core][meta]" ) {
  STATIC_REQUIRE( intToStrSuccessful() == true );
}

}

}
