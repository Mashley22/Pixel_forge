module;

#include <chrono>

#include <PixelForge/core/macros.hpp>

export module PixelForge.logging:backends.fast_backend;

import PixelForge.core;
import :record;

export namespace pf::log {

/**
 *@brief A fast logger backend. Each thread owns its own logging instance
 *       and a background thread collects logs from each thread and writes
 *       them to a log file (WITHOUT SORTING). The log file should later be
 *       serialised
 */
class FastBackend {

  using Clock = std::chrono::system_clock;

  using TimePoint_t = Clock::time_point;

  static_assert(sizeof(TimePoint_t) == sizeof(std::uint64_t));

  struct Header {
    TimePoint_t time{};
    std::uint32_t id{};
    std::uint16_t size{0};
    Level level{Level::DEBUG};
    bool isLast{};
  };

  static_assert(sizeof(Header) == 2 * sizeof(TimePoint_t));
  static_assert(alignof(Header) == alignof(TimePoint_t));

  struct Payload {
    Header header;
    const char* data{nullptr};
  };
};

}
