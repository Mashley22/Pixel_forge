module;

#include <chrono>
#include <cstdint>

#include <PixelForge/core/macros.hpp>

export module PixelForge.logging:backend.fast.types;

import PixelForge.core;
import PixelForge.containers;
import PixelForge.mem;
import :record;

namespace pf::log::fast_backend {

export using clock = std::chrono::system_clock;
export using size_type = std::size_t;

export using time_point_t = clock::time_point;

export constexpr std::size_t block_size = PIXELFORGE_FAST_LOG_BACKEND_BLOCK_SIZE;

static_assert(sizeof(time_point_t) == sizeof(std::uint64_t));

export struct Header {
  time_point_t time{};
  std::uint32_t id{};
  std::uint16_t size{0};
  Level level{Level::DEBUG};
  bool is_last{false};
};

static_assert(sizeof(Header) == 2 * sizeof(time_point_t));
static_assert(alignof(Header) == alignof(time_point_t));

export struct Payload {
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

}
