#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string_view>
#include <type_traits>

import PixelForge.core;

namespace pf {

namespace {
constexpr std::string_view dummyStrView = "";
}

TEST_CASE("policy concepts", "[core][errPolicy]") {
  STATIC_REQUIRE(ErrPolicy_c<ErrPolicy_nothing<int, dummyStrView>, int>);
  STATIC_REQUIRE(ErrPolicy_c<ErrPolicy_optional<int>, int>);
  STATIC_REQUIRE(ErrPolicy_c<ErrPolicy_throws<int, Exception>, int>);

  STATIC_REQUIRE(!ErrPolicy_c<ErrPolicy_nothing<void, dummyStrView>, void>);
  STATIC_REQUIRE(!ErrPolicy_c<ErrPolicy_optional<void>, void>);
  STATIC_REQUIRE(!ErrPolicy_c<ErrPolicy_throws<void, Exception>, void>);

  STATIC_REQUIRE(VoidErrPolicy_c<ErrPolicy_nothing<void, dummyStrView>>);
  STATIC_REQUIRE(VoidErrPolicy_c<ErrPolicy_optional<void>>);
  STATIC_REQUIRE(VoidErrPolicy_c<ErrPolicy_throws<void, Exception>>);
}

TEST_CASE("nothing policy", "[core][errPolicy]") {
  STATIC_REQUIRE(ErrPolicy_nothing<int, dummyStrView>::is_noexcept);
  STATIC_REQUIRE(std::is_same_v<ErrPolicy_nothing<int, dummyStrView>::return_type, int>);

  REQUIRE(ErrPolicy_nothing<int, dummyStrView>::success(42) == 42);

  int lvalue = 7;
  REQUIRE(ErrPolicy_nothing<int, dummyStrView>::success(lvalue) == 7);
}

TEST_CASE("nothing policy fail requires", "[core][errPolicy]") {
  // PF_REQUIRE(false) throws RequireFail when
  // PIXELFORGE_REQUIRE_THROWS_ON_FAILURE is defined
  auto dummy = []() { ErrPolicy_nothing<int, dummyStrView>::fail(0); };
  REQUIRE_THROWS_AS(dummy(), RequireFail);
}

TEST_CASE("nothing policy void", "[core][errPolicy]") {
  STATIC_REQUIRE(
      std::is_same_v<ErrPolicy_nothing<void, dummyStrView>::return_type, void>);

  REQUIRE_NOTHROW(ErrPolicy_nothing<void, dummyStrView>::success());
  auto dummy = []() { ErrPolicy_nothing<int, dummyStrView>::fail(0); };
  REQUIRE_THROWS_AS(dummy(), RequireFail);
}

TEST_CASE("optional policy", "[core][errPolicy]") {
  STATIC_REQUIRE(ErrPolicy_optional<int>::is_noexcept);
  STATIC_REQUIRE(
      std::is_same_v<ErrPolicy_optional<int>::return_type, std::optional<int>>);

  REQUIRE(ErrPolicy_optional<int>::success(42) == std::optional<int>{42});
  REQUIRE_FALSE(ErrPolicy_optional<int>::fail("some reason").has_value());
}

TEST_CASE("optional policy void", "[core][errPolicy]") {
  STATIC_REQUIRE(std::is_same_v<ErrPolicy_optional<void>::return_type, bool>);

  REQUIRE(ErrPolicy_optional<void>::success());
  REQUIRE_FALSE(ErrPolicy_optional<void>::fail("some reason"));
}

TEST_CASE("throws policy", "[core][errPolicy]") {
  STATIC_REQUIRE(!ErrPolicy_throws<int, Exception>::is_noexcept);
  STATIC_REQUIRE(std::is_same_v<ErrPolicy_throws<int, Exception>::return_type, int>);

  REQUIRE(ErrPolicy_throws<int, Exception>::success(42) == 42);

  try {
    ErrPolicy_throws<int, Exception>::fail("boom");
    REQUIRE_FALSE(true);
  } catch (const Exception& e) {
    REQUIRE(std::string_view{e.what()} == "boom");
  }
}

TEST_CASE("throws policy builtin exception type", "[core][errPolicy]") {
  REQUIRE_THROWS_AS((ErrPolicy_throws<int, int>::fail(7)), int);
}

TEST_CASE("throws policy void", "[core][errPolicy]") {
  STATIC_REQUIRE(std::is_same_v<ErrPolicy_throws<void, Exception>::return_type, void>);

  REQUIRE_NOTHROW(ErrPolicy_throws<void, Exception>::success());

  try {
    ErrPolicy_throws<void, Exception>::fail("void boom");
    REQUIRE_FALSE(true);
  } catch (const Exception& e) {
    REQUIRE(std::string_view{e.what()} == "void boom");
  }
}

}
