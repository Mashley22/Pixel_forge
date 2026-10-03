#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string_view>
#include <type_traits>

import PixelForge.core;

#include <PixelForgeValidationHelpers/helpers.hpp>

namespace pf {

namespace {
constexpr std::string_view dummyStrView = "";
}

PF_TEST_CASE("policy concepts", "[core][errPolicy]") {
  STATIC_REQUIRE(ErrPolicy_c<ErrPolicyNothing<int, dummyStrView>, int>);
  STATIC_REQUIRE(ErrPolicy_c<ErrPolicyOptional<int>, int>);
  STATIC_REQUIRE(ErrPolicy_c<ErrPolicyThrows<int, Exception>, int>);

  STATIC_REQUIRE(!ErrPolicy_c<ErrPolicyNothing<void, dummyStrView>, void>);
  STATIC_REQUIRE(!ErrPolicy_c<ErrPolicyOptional<void>, void>);
  STATIC_REQUIRE(!ErrPolicy_c<ErrPolicyThrows<void, Exception>, void>);

  STATIC_REQUIRE(VoidErrPolicy_c<ErrPolicyNothing<void, dummyStrView>>);
  STATIC_REQUIRE(VoidErrPolicy_c<ErrPolicyOptional<void>>);
  STATIC_REQUIRE(VoidErrPolicy_c<ErrPolicyThrows<void, Exception>>);
}

PF_TEST_CASE("nothing policy", "[core][errPolicy]") {
  STATIC_REQUIRE(ErrPolicyNothing<int, dummyStrView>::is_noexcept);
  STATIC_REQUIRE(std::is_same_v<ErrPolicyNothing<int, dummyStrView>::return_type, int>);

  REQUIRE(ErrPolicyNothing<int, dummyStrView>::success(42) == 42);

  int lvalue = 7;
  REQUIRE(ErrPolicyNothing<int, dummyStrView>::success(lvalue) == 7);
}

PF_TEST_CASE("nothing policy fail requires", "[core][errPolicy]") {
  auto dummy = []() { ErrPolicyNothing<int, dummyStrView>::fail(0); };
  REQUIRE_PF_REQUIRE_FAIL(dummy());
}

PF_TEST_CASE("nothing policy void", "[core][errPolicy]") {
  STATIC_REQUIRE(std::is_same_v<ErrPolicyNothing<void, dummyStrView>::return_type, void>);

  REQUIRE_NOTHROW(ErrPolicyNothing<void, dummyStrView>::success());
  auto dummy = []() { ErrPolicyNothing<int, dummyStrView>::fail(0); };
  REQUIRE_PF_REQUIRE_FAIL(dummy());
}

PF_TEST_CASE("optional policy", "[core][errPolicy]") {
  STATIC_REQUIRE(ErrPolicyOptional<int>::is_noexcept);
  STATIC_REQUIRE(std::is_same_v<ErrPolicyOptional<int>::return_type, std::optional<int>>);

  REQUIRE(ErrPolicyOptional<int>::success(42) == std::optional<int>{42});
  REQUIRE_FALSE(ErrPolicyOptional<int>::fail("some reason").has_value());
}

PF_TEST_CASE("optional policy void", "[core][errPolicy]") {
  STATIC_REQUIRE(std::is_same_v<ErrPolicyOptional<void>::return_type, bool>);

  REQUIRE(ErrPolicyOptional<void>::success());
  REQUIRE_FALSE(ErrPolicyOptional<void>::fail("some reason"));
}

PF_TEST_CASE("throws policy", "[core][errPolicy]") {
  STATIC_REQUIRE(!ErrPolicyThrows<int, Exception>::is_noexcept);
  STATIC_REQUIRE(std::is_same_v<ErrPolicyThrows<int, Exception>::return_type, int>);

  REQUIRE(ErrPolicyThrows<int, Exception>::success(42) == 42);

  try {
    ErrPolicyThrows<int, Exception>::fail("boom");
    REQUIRE_FALSE(true);
  } catch (const Exception& e) {
    REQUIRE(std::string_view{e.what()} == "boom");
  }
}

PF_TEST_CASE("throws policy builtin exception type", "[core][errPolicy]") {
  REQUIRE_THROWS_AS((ErrPolicyThrows<int, int>::fail(7)), int);
}

PF_TEST_CASE("throws policy void", "[core][errPolicy]") {
  STATIC_REQUIRE(std::is_same_v<ErrPolicyThrows<void, Exception>::return_type, void>);

  REQUIRE_NOTHROW(ErrPolicyThrows<void, Exception>::success());

  try {
    ErrPolicyThrows<void, Exception>::fail("void boom");
    REQUIRE_FALSE(true);
  } catch (const Exception& e) {
    REQUIRE(std::string_view{e.what()} == "void boom");
  }
}

}
