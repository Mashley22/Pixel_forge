#include <map>
#include <utility>

#include <PixelForgeValidationHelpers/helpers.hpp>
#include <catch2/catch_test_macros.hpp>

import PixelForge.core;

namespace pf::meta {

template <typename T, typename U>
concept SameType = std::same_as<T, U>;

template <typename T>
struct Wrap1 {
  using type = T;
};
template <typename T1, typename T2>
struct Wrap2 {
  using t1 = T1;
  using t2 = T2;
};

PF_TEST_CASE("rebind", "[core][meta]") {
  SECTION("single-arg") {
    using R = Rebind<Wrap1<int>>::to<double>;
    STATIC_REQUIRE(SameType<R, Wrap1<double>>);

    using R2 = Rebind<Wrap1<char>>::to<float>;
    STATIC_REQUIRE(SameType<R2, Wrap1<float>>);
  }

  SECTION("two-arg") {
    using R = Rebind<Wrap2<int, double>>::to<float, char>;
    STATIC_REQUIRE(SameType<R, Wrap2<float, char>>);

    using R2 = Rebind<Wrap2<int, char>>::to<double, float>;
    STATIC_REQUIRE(SameType<R2, Wrap2<double, float>>);
  }

  SECTION("chained") {
    using S1 = Rebind<Wrap1<int>>::to<double>;
    using S2 = Rebind<S1>::to<char>;
    STATIC_REQUIRE(SameType<S2, Wrap1<char>>);
  }

  SECTION("static_asserts") {
    static_assert(std::is_same_v<Rebind<Wrap1<char>>::to<int>, Wrap1<int>>);
    static_assert(
        std::is_same_v<Rebind<Wrap2<int, int>>::to<float, char>, Wrap2<float, char>>);
  }
}

} // namespace pf::meta
