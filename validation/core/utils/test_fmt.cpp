#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

import PixelForge.core;

constexpr auto BUF_SIZE = 64;

namespace pf {

namespace {

template <class... V_args>
void
M_testFmtAgainstStd(std::format_string<V_args...> str, V_args&&... args) {
  const auto pfRes = fmt<BUF_SIZE>(str, std::forward<V_args>(args)...);
  const auto stdRes = std::format(str, std::forward<V_args>(args)...);
  const std::string pfStr(pfRes.str, pfRes.size);
  const std::string stdStr(pfRes.str, pfRes.size);

  REQUIRE(pfStr == stdStr);
}

template <class... V_args>
void
M_testFmt_cstrAgainstStd(std::format_string<V_args...> str, V_args&&... args) {
  const auto pfRes = fmt_cstr<BUF_SIZE>(str, std::forward<V_args>(args)...);
  const auto stdRes = std::format(str, std::forward<V_args>(args)...);

  const std::string pfStr(pfRes.str);
  const std::string stdStr(pfRes.str);

  REQUIRE(pfRes.str[pfRes.size] == '\0');
  REQUIRE(pfStr == stdStr);
}

TEST_CASE("fmt", "[core][utils][fmt]") {
  SECTION("Plain string") { M_testFmtAgainstStd("Plain string"); }
  SECTION("With some formatting") { M_testFmtAgainstStd("{}", 1); }
}

TEST_CASE("fmt_cstr", "[core][utils][fmt]") {
  SECTION("Plain string") { M_testFmt_cstrAgainstStd("Nothing"); }
  SECTION("With some formatting") { M_testFmtAgainstStd("{}", 1); }
}

}

}
