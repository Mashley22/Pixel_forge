module;

#include <chrono>

#include <PixelForge/core/macros.hpp>

export module PixelForge.logging:backends.fast_backend;

import PixelForge.core;
import PixelForge.containers;
import PixelForge.mem;
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

  using TimePointT = Clock::time_point;

  static_assert(sizeof(TimePointT) == sizeof(std::uint64_t));

  struct Header {
    TimePointT time{};
    std::uint32_t id{};
    std::uint16_t size{0};
    Level level{Level::DEBUG};
    bool is_last{};
  };

  static_assert(sizeof(Header) == 2 * sizeof(TimePointT));
  static_assert(alignof(Header) == alignof(TimePointT));

  struct Payload {
    Header header;

    [[nodiscard]] std::span<const char>
    data() const PF_NOEXCEPT {
      return std::span<const char>(pointer_cast<const char*>(this + 1), header.size);
    }

    [[nodiscard]] std::span<char>
    data() PF_NOEXCEPT {
      return std::span<char>(pointer_cast<char*>(this + 1), header.size);
    }
  };
};

}
