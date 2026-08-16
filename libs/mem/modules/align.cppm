module;

#include <cstdint>
#include <cstddef>

#include <PixelForge/core/macros.hpp>

export module PixelForge.mem.align;

import PixelForge.core;

namespace pf {

export namespace mem {

/**@brief Invalid if alignment is not a power of 2 or ptr is nullptr
  */
 [[nodiscard]]
constexpr std::byte*
align(std::byte* ptr,
      std::size_t alignment) PF_NOEXCEPT {
  PF_REQUIRE_ASSUME(ptr != nullptr, "nullptr is not valid here!");
  PF_REQUIRE(math::isPowerOfTwo(alignment));
  
  const auto addr = reinterpret_cast<std::uintptr_t>(ptr);

  const auto alignment_padding = (alignment - (addr & (alignment - 1))) & (alignment - 1);

  return ptr + alignment_padding;
}

}

}
