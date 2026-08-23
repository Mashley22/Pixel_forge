#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

import PixelForge.logging;

namespace pf::log {

namespace {

constexpr auto LONG_MSG_LEN = 128;
constexpr auto THREADS = 4;
constexpr auto PER_THREAD = 250;

constexpr std::string_view THREAD_MSGS[THREADS] = {"t0msg", "t1msg", "t2msg", "t3msg"};

Record
makeRecord(Level lvl, std::string_view msg) {
  return {.time = std::chrono::system_clock::now(), .level = lvl, .id = 0xAB, .msg = msg};
}

}

TEST_CASE("ConsoleBackend satisfies Backend_c", "[log][console]") {
  STATIC_REQUIRE(Backend_c<ConsoleBackend>);
}

TEST_CASE("ConsoleBackend formatLine", "[log][console]") {

  SECTION("renders timestamp, level, id and message") {
    const Record rec = makeRecord(Level::ERROR, "boom");
    std::array<char, ConsoleBackend::lineBufSize> buf{};
    const std::size_t n = ConsoleBackend::formatLine(rec, buf);

    REQUIRE(n < buf.size());
    const std::string_view out(buf.data(), n);

    // [YYYY-MM-DD HH:MM:SS(.fraction)?] [ERR] [thread-AB] boom
    REQUIRE(out.starts_with('['));
    REQUIRE(out[5] == '-');
    REQUIRE(out[8] == '-');
    REQUIRE(out[14] == ':');
    REQUIRE(out[17] == ':');
    const std::size_t stampEnd = out.find(']', 20);
    REQUIRE(stampEnd != std::string_view::npos);
    REQUIRE(out.substr(stampEnd).starts_with("] [ERR]"));
    REQUIRE(out.find("[thread-AB]") != std::string_view::npos);
    REQUIRE(out.ends_with("boom"));
  }

  SECTION("level tags map through toStr") {
    std::array<char, ConsoleBackend::lineBufSize> buf{};

    const std::size_t dbgLen =
        ConsoleBackend::formatLine(makeRecord(Level::DEBUG, "x"), buf);
    REQUIRE(std::string_view(buf.data(), dbgLen).find("[DBG]") != std::string_view::npos);

    const std::size_t warnLen =
        ConsoleBackend::formatLine(makeRecord(Level::WARNING, "x"), buf);
    REQUIRE(std::string_view(buf.data(), warnLen).find("[WRN]") !=
            std::string_view::npos);

    const std::size_t infLen =
        ConsoleBackend::formatLine(makeRecord(Level::INFO, "x"), buf);
    REQUIRE(std::string_view(buf.data(), infLen).find("[INF]") != std::string_view::npos);
  }

  SECTION("long messages report the untruncated size") {
    const std::string longMsg(LONG_MSG_LEN, 'x');
    const Record rec = makeRecord(Level::INFO, longMsg);

    std::array<char, 64> smallBuf{};
    const std::size_t n = ConsoleBackend::formatLine(rec, smallBuf);

    // full line would be ~41 prefix chars + message, well past 64
    REQUIRE(n > smallBuf.size());

    const std::string_view written(smallBuf.data(), smallBuf.size());
    REQUIRE(written.starts_with('['));
    REQUIRE(written.ends_with('x'));
  }
}

TEST_CASE("ConsoleBackend log writes every level", "[log][console]") {
  ConsoleBackend::log(makeRecord(Level::DEBUG, "debug line"));
  ConsoleBackend::log(makeRecord(Level::INFO, "info line"));
  ConsoleBackend::log(makeRecord(Level::WARNING, "warning line"));
  ConsoleBackend::log(makeRecord(Level::ERROR, "error line"));
  REQUIRE(true);
}

TEST_CASE("ConsoleBackend log is line-atomic under concurrency", "[log][console]") {
  // redirect std::cout and std::cerr into one in-memory buffer
  std::ostringstream captured;
  auto* const oldOut = std::cout.rdbuf(captured.rdbuf());
  auto* const oldErr = std::cerr.rdbuf(captured.rdbuf());

  {
    std::vector<std::jthread> threads;
    for (std::size_t t = 0; t < THREADS; t++) {
      threads.emplace_back([t] {
        for (int i = 0; i < PER_THREAD; i++) {
          ConsoleBackend::log(makeRecord(Level::DEBUG, THREAD_MSGS[t]));
        }
      });
    }
  }

  std::cout.rdbuf(oldOut);
  std::cerr.rdbuf(oldErr);

  std::size_t counts[THREADS]{};
  std::size_t totalLines = 0;
  std::istringstream stream(captured.str());
  std::string line;
  while (std::getline(stream, line)) {
    if (line.empty()) {
      continue;
    }
    totalLines++;

    bool attributed = false;
    for (std::size_t t = 0; t < THREADS; t++) {
      if (line.ends_with(THREAD_MSGS[t])) {
        counts[t]++;
        attributed = true;
      }
    }

    // a torn or merged line would match no thread's tag
    REQUIRE(attributed);
    REQUIRE(line.find("[DBG]") != std::string::npos);
    REQUIRE(line.find("[thread-AB]") != std::string::npos);
  }

  REQUIRE(totalLines == THREADS * PER_THREAD);
  for (std::size_t t = 0; t < THREADS; t++) {
    REQUIRE(counts[t] == PER_THREAD);
  }
}

}
