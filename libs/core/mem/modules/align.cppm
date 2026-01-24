module;

#include <cstdint>
#include <cstddef>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.mem.align;

import PixelForge.core.require;
import PixelForge.core.math;

namespace pf {

export namespace mem {

/**@brief Invalid if alignment is not a power of 2 or ptr is nullptr
  */
 [[nodiscard]]
constexpr std::byte*
align(std::byte* ptr,
      std::size_t alignment) PF_NOEXCEPT {
  require(ptr != nullptr, "nullptr is not valid here!");
  require(math::isPowerOfTwo(alignment));
  
  const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
  
  const auto aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
  
  return reinterpret_cast<std::byte*>(aligned_addr);
}

}

}
