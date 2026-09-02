#include <PixelForgeValidationHelpers/helpers.hpp>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include <PixelForgeValidationHelpers/helpers.hpp>

import PixelForge.mem;

import PixelForge.core;

constexpr auto NON_ZERO_VALUE = 8;
constexpr auto TEST_ARR_LEN = 1000;

static_assert(NON_ZERO_VALUE != 0);

namespace pf::mem {

namespace {

struct NotZeroByte {
  std::byte data{8};
};

}

PF_TEST_CASE("memzero", "[core][mem]") {

  SECTION("basic usage") {
    NotZeroByte arr[TEST_ARR_LEN] = {};

    memzero(arr, TEST_ARR_LEN * sizeof(NotZeroByte));

    for (auto& i : arr) {
      REQUIRE(i.data == std::byte{0});
    }
  }

  SECTION("no null") {
    REQUIRE_PF_REQUIRE_FAIL(memzero(reinterpret_cast<void*>(NON_ZERO_VALUE), 0));
    REQUIRE_PF_REQUIRE_FAIL(memzero(reinterpret_cast<void*>(0), NON_ZERO_VALUE));
  }

  SECTION("span<char>") {
    char arr[TEST_ARR_LEN] = {static_cast<char>(NON_ZERO_VALUE)};

    memzero(std::span<char>{arr});

    for (auto c : arr) {
      REQUIRE(c == char{0});
    }
  }

  SECTION("span<std::byte>") {
    std::byte arr[TEST_ARR_LEN] = {std::byte{NON_ZERO_VALUE}};

    memzero(std::span<std::byte>{arr});

    for (auto b : arr) {
      REQUIRE(b == std::byte{0});
    }
  }

  SECTION("span no null") {
    REQUIRE_PF_REQUIRE_FAIL(
        memzero(std::span<char>{reinterpret_cast<char*>(NON_ZERO_VALUE), 0}));
    REQUIRE_PF_REQUIRE_FAIL(
        memzero(std::span<char>{reinterpret_cast<char*>(NULL), NON_ZERO_VALUE}));
    REQUIRE_PF_REQUIRE_FAIL(
        memzero(std::span<std::byte>{reinterpret_cast<std::byte*>(NON_ZERO_VALUE), 0}));
    REQUIRE_PF_REQUIRE_FAIL(memzero(
        std::span<std::byte>{reinterpret_cast<std::byte*>(NULL), NON_ZERO_VALUE}));
  }
}

}
