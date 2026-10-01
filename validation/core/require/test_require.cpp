#include <limits>
#include <source_location>
#include <string>
#include <thread>
#include <vector>

#include <PixelForgeValidationHelpers/helpers.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

#ifdef __clang__
#define ANONYMOUS_NAMESPACE "(anonymous namespace)"
#elifdef __GNUC__
#define ANONYMOUS_NAMESPACE "{anonymous}"
#endif

import PixelForge.core;

namespace pf {

namespace {

void
M_testRequireInfos() {
  std::string_view msg = "funny message here";
  try {
    require(false, msg);
    REQUIRE_FALSE(true);
  } catch (RequireFail& e) {
    REQUIRE(e.msg == msg);
    // a lil bit of magic
    REQUIRE_THAT(e.loc.file_name(),
                 Catch::Matchers::ContainsSubstring("test_require.cpp"));
    REQUIRE(e.loc.line() == 27);
    REQUIRE_THAT(e.loc.function_name(),
                 Catch::Matchers::Equals("void pf::" ANONYMOUS_NAMESPACE
                                         "::M_testRequireInfos()"));
    // column check issues
  }
}

}

PF_TEST_CASE("basic", "[core][assert]") {
  REQUIRE_NOTHROW(require(true));

  try {
    require(false);
    REQUIRE_FALSE(true);
  } catch (RequireFail& e) {
    (void) e;
  }
}

PF_TEST_CASE("correct infos", "[core][assert]") { M_testRequireInfos(); }

PF_TEST_CASE("logContinue records a fail", "[core][assert]") {
  std::string msg = "log and continue";
  const auto loc = std::source_location::current();
  const auto idxBefore = RequireFailLogContinue::current_idx();

  RequireFailLogContinue::fail(msg, loc);

  REQUIRE(RequireFailLogContinue::current_idx() == idxBefore + 1);
  const auto& last = RequireFailLogContinue::get_last_error();
  REQUIRE(last.msg == msg);
  REQUIRE(last.loc.file_name() == loc.file_name());
  REQUIRE(last.loc.line() == loc.line());
  REQUIRE(last.loc.column() == loc.column());
  REQUIRE(RequireFailLogContinue::fail_infos()[RequireFailLogContinue::current_idx() %
                                               PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE]
              .msg == msg);
}

PF_TEST_CASE("logContinue is a circular buffer", "[core][assert]") {
  const std::size_t size = PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE;
  std::vector<std::string> msgs;
  msgs.reserve(size);
  for (std::size_t i = 0; i < size; ++i) {
    msgs.push_back("fail #" + std::to_string(i));
  }

  const auto base = RequireFailLogContinue::current_idx();
  for (std::size_t i = 0; i < size; ++i) {
    RequireFailLogContinue::fail(msgs[i], std::source_location::current());
  }

  REQUIRE(RequireFailLogContinue::current_idx() == base + size);
  REQUIRE(RequireFailLogContinue::get_last_error().msg == msgs[size - 1]);
  const auto infos = RequireFailLogContinue::fail_infos();
  for (std::size_t i = 0; i < size; ++i) {
    REQUIRE(infos[(base + i + 1) % size].msg == msgs[i]);
  }
}

PF_TEST_CASE("logContinue is thread local", "[core][assert]") {
  std::string mainMsg = "main thread fail";
  std::string workerMsg = "worker thread fail";

  RequireFailLogContinue::fail(mainMsg, std::source_location::current());
  const auto mainIdx = RequireFailLogContinue::current_idx();
  REQUIRE(RequireFailLogContinue::get_last_error().msg == mainMsg);

  std::size_t workerIdxBefore = 0;
  std::size_t workerIdxAfter = 0;
  std::string workerLastMsg;
  bool workerStartedEmpty = false;

  std::thread worker([&] {
    workerIdxBefore = RequireFailLogContinue::current_idx();
    workerStartedEmpty = RequireFailLogContinue::get_last_error().empty();
    RequireFailLogContinue::fail(workerMsg, std::source_location::current());
    workerIdxAfter = RequireFailLogContinue::current_idx();
    workerLastMsg = std::string(RequireFailLogContinue::get_last_error().msg);
  });
  worker.join();

  REQUIRE(workerIdxBefore == std::numeric_limits<std::size_t>::max());
  REQUIRE(workerStartedEmpty);
  REQUIRE(workerIdxAfter == 0);
  REQUIRE(workerLastMsg == workerMsg);

  REQUIRE(RequireFailLogContinue::current_idx() == mainIdx);
  REQUIRE(RequireFailLogContinue::get_last_error().msg == mainMsg);
}

}
