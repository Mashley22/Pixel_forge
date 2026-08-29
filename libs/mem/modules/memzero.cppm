module;

#include <cstddef>
#include <span>

#include <PixelForge/core/macros.hpp>

export module PixelForge.mem:memzero;

import PixelForge.core;

export namespace pf::mem {

constexpr void
memzero(void* dest, std::size_t count) PF_NOEXCEPT {
  PF_REQUIRE(dest != nullptr);
  PF_REQUIRE(count != 0);

  volatile std::byte* p = static_cast<volatile std::byte*>(dest);

  for (std::size_t i = 0; i < count; i++) {
    p[i] = std::byte{0};
  }
}

constexpr void
memzero(std::span<char> buf) PF_NOEXCEPT {
  PF_REQUIRE(buf.data() != nullptr);
  PF_REQUIRE(buf.size() != 0);
  memzero(buf.data(), buf.size());
}

constexpr void
memzero(std::span<std::byte> buf) PF_NOEXCEPT {
  PF_REQUIRE(buf.data() != nullptr);
  PF_REQUIRE(buf.size() != 0);
  memzero(buf.data(), buf.size());
}

}
