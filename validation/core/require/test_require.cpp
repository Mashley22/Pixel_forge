#include <source_location>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

import PixelForge.core.require;

namespace pf {

namespace {

void
M_testRequireInfos(void) {
  std::string_view msg = "funny message here";
  try {
    require(false, msg);
    REQUIRE_FALSE(true);
  }
  catch(RequireFail& e) {
    REQUIRE(e.msg == msg);
    // a lil bit of magic
    REQUIRE_THAT(e.loc.file_name(), Catch::Matchers::ContainsSubstring("test_require.cpp"));
    REQUIRE(e.loc.line() == 16);
    REQUIRE_THAT(e.loc.function_name(), Catch::Matchers::Equals("void pf::(anonymous namespace)::M_testRequireInfos()")); // A bit unsure how guaranteed this is
    REQUIRE(e.loc.column() == 5);
  }
}

}

TEST_CASE( "basic", "[core][assert]") {
  REQUIRE_NOTHROW(require(true));

  try {
    require(false);
    REQUIRE_FALSE(true);
  }
  catch(RequireFail& e) {
    (void)e;
  }
}

TEST_CASE( "correct infos", "[core][assert]") {
  M_testRequireInfos();
}

}
