module;

#include <chrono>

#include <PixelForge/core/macros.hpp>

export module PixelForge.logging:backend.fast;

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

  using clock = std::chrono::system_clock;
  using size_type = std::size_t;

  using time_point_t = clock::time_point;

  static_assert(sizeof(time_point_t) == sizeof(std::uint64_t));

  struct Header {
    time_point_t time{};
    std::uint32_t id{};
    std::uint16_t size{0};
    Level level{Level::DEBUG};
    bool is_last{false};
  };

  static_assert(sizeof(Header) == 2 * sizeof(time_point_t));
  static_assert(alignof(Header) == alignof(time_point_t));

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

  class Collector {
    public:
      static constexpr size_type block_size = 
#ifdef PIXELFORGE_FAST_LOG_BACKEND_COLLECTOR_POOL_BLOCK_SIZE
        PIXELFORGE_FAST_LOG_BACKEND_COLLECTOR_POOL_BLOCK_SIZE;
#else
        2048;
#endif
  };


};

}
