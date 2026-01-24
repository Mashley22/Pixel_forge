import PixelForge.core.mem;
import PixelForge.core.require;

#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#define CAST_TO_BYTE_PTR(val) reinterpret_cast<std::byte*>((void*)val);

namespace pf {

namespace mem {

TEST_CASE( "align", "[core][mem]" ) {

  std::byte * p_val = CAST_TO_BYTE_PTR(5);
  REQUIRE(p_val != nullptr);

  SECTION( "require fail for nullptr input" ) {
    REQUIRE_THROWS_AS(align(nullptr, 1), RequireFail);
  }

  SECTION( "require fail for non power of two alignments" ) {
    auto test = [&](std::size_t alignment) {
      REQUIRE_THROWS_AS(align(p_val, alignment), RequireFail);
    };

    constexpr std::size_t invalidAlignments[] = {
      0, 3, 9, 199, 3789, 1203981905
    };

    for (const auto alignment : invalidAlignments) {
      test(alignment);
    }
  }

  SECTION( "some valid inputs" ) {

    auto test = [](std::size_t start, std::size_t alignment, std::size_t res) {
      std::byte * lhs = CAST_TO_BYTE_PTR(start);
      std::byte * rhs = CAST_TO_BYTE_PTR(res);
      REQUIRE(align(lhs, alignment) == rhs);
    };

    test(10, 1, 10);
    test(10, 4, 12);
    test(100, 4, 100);
    test(99, 4, 100);
    test(255, 64, 256);
  }

}

}

}
