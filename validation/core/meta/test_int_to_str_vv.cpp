#include <array>
#include <cstdint>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

import PixelForge.core;

constexpr auto TEST_VALS_NUM = 3;

namespace pf {

namespace meta {

namespace {

constexpr std::array<int, TEST_VALS_NUM> testInts = {5, 9, -420};

constexpr std::array<std::string_view, TEST_VALS_NUM> testStrs = {"5", "9", "-420"};

auto strToIntSuccessful = []() {
  for (std::size_t i = 0; i < TEST_VALS_NUM; i++) {
    if (strToInt<int>(testStrs[i]) != testInts[i]) {
      return false;
    }
  }

  return true;
};

template <std::size_t T_idx = testInts.size()>
[[nodiscard]]
consteval bool
intToStrSuccessful() {
  if (!intToStrSuccessful<T_idx - 1>()) {
    return false;
  }

  return IntToStr<int, 1>::sv() == "1"; // testVals[T_idx].str;
};

template <>
[[nodiscard]]
consteval bool
intToStrSuccessful<0>() {
  return IntToStr<int, testInts[0]>::sv() == testStrs[0];
}

}

TEST_CASE("test svToInt", "[core][meta]") {
  STATIC_REQUIRE(strToIntSuccessful() == true);
}

TEST_CASE("test intToSv", "[core][meta]") {
  STATIC_REQUIRE(intToStrSuccessful() == true);
}

TEST_CASE("test UintToStr multi digit", "[core][meta]") {
  STATIC_REQUIRE(UintToStr<unsigned int, 420>::sv() == "420");
  STATIC_REQUIRE(UintToStr<unsigned int, 1000>::sv() == "1000");
  STATIC_REQUIRE(UintToStr<unsigned int, 10>::sv() == "10");
  STATIC_REQUIRE(UintToStr<unsigned int, 9>::sv() == "9");
  STATIC_REQUIRE(UintToStr<unsigned int, 0>::sv() == "0");
  STATIC_REQUIRE(UintToStr<unsigned long long, 18446744073709551615ULL>::sv() ==
                 "18446744073709551615");
  STATIC_REQUIRE(UintToStr<std::uint8_t, 255, 16>::sv() == "FF");
  STATIC_REQUIRE(UintToStr<unsigned int, 36>::sv() == "36");
}

TEST_CASE("test IntToStr negatives and edge cases", "[core][meta]") {
  STATIC_REQUIRE(IntToStr<int, -420>::sv() == "-420");
  STATIC_REQUIRE(IntToStr<int, -1>::sv() == "-1");
  STATIC_REQUIRE(IntToStr<int, -2147483647 - 1>::sv() == "-2147483648");
  STATIC_REQUIRE(IntToStr<int, 2147483647>::sv() == "2147483647");
  STATIC_REQUIRE(IntToStr<int, 0>::sv() == "0");
}

}

}
