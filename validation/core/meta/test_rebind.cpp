#include <map>
#include <utility>

#include <PixelForgeValidationHelpers/helpers.hpp>
#include <catch2/catch_test_macros.hpp>

import PixelForge.core;

namespace pf::meta {

template <typename Left, typename Right>
concept SameType = std::same_as<Left, Right>;

template <typename Arg>
struct Wrap1 {
  using Type = Arg;
};
template <typename Arg1, typename Arg2>
struct Wrap2 {
  using First = Arg1;
  using Second = Arg2;
};

PF_TEST_CASE("rebind", "[core][meta]") {
  SECTION("single-arg") {
    using R = Rebind<Wrap1<int>>::To<double>;
    STATIC_REQUIRE(SameType<R, Wrap1<double>>);

    using R2 = Rebind<Wrap1<char>>::To<float>;
    STATIC_REQUIRE(SameType<R2, Wrap1<float>>);
  }

  SECTION("two-arg") {
    using R = Rebind<Wrap2<int, double>>::To<float, char>;
    STATIC_REQUIRE(SameType<R, Wrap2<float, char>>);

    using R2 = Rebind<Wrap2<int, char>>::To<double, float>;
    STATIC_REQUIRE(SameType<R2, Wrap2<double, float>>);
  }

  SECTION("chained") {
    using S1 = Rebind<Wrap1<int>>::To<double>;
    using S2 = Rebind<S1>::To<char>;
    STATIC_REQUIRE(SameType<S2, Wrap1<char>>);
  }

  SECTION("static_asserts") {
    static_assert(std::is_same_v<Rebind<Wrap1<char>>::To<int>, Wrap1<int>>);
    static_assert(
        std::is_same_v<Rebind<Wrap2<int, int>>::To<float, char>, Wrap2<float, char>>);
  }
}

} // namespace pf::meta
