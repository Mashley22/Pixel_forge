#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <vector>

import PixelForge.core;

namespace pf {

namespace meta {

namespace {

template <typename T, typename U>
concept SameType = std::same_as<T, U>;

}

TEST_CASE("rebind: Basic type rebinding", "[core][meta]") {
  using ReboundVector = Rebind<std::vector<int>>::to<double>;
  STATIC_REQUIRE((SameType<ReboundVector, std::vector<double>>) );

  using ReboundUniquePtr = Rebind<std::unique_ptr<int>>::to<float>;
  STATIC_REQUIRE((SameType<ReboundUniquePtr, std::unique_ptr<float>>) );

  using ReboundString = Rebind<std::basic_string<char>>::to<wchar_t>;
  STATIC_REQUIRE((SameType<ReboundString, std::basic_string<wchar_t>>) );
}

TEST_CASE("rebind: Multiple rebind operations", "[core][meta]") {
  using Step1 = Rebind<std::vector<int>>::to<double>;
  using Step2 = Rebind<Step1>::to<float>;
  STATIC_REQUIRE((SameType<Step2, std::vector<float>>) );
}

TEST_CASE("rebind: Verify the 'to' alias directly", "[core][meta]") {
  static_assert(std::is_same_v<Rebind<std::vector<char>>::to<int>, std::vector<int>>);
  static_assert(
      std::is_same_v<Rebind<std::unique_ptr<long>>::to<double>, std::unique_ptr<double>>);
  static_assert(
      std::is_same_v<Rebind<std::basic_string<char>>::to<int>, std::basic_string<int>>);
  static_assert(std::is_same_v<Rebind<std::vector<bool>>::to<char>, std::vector<char>>);
}

}

}
