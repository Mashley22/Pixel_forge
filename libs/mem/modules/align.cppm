module;

#include <bit>
#include <cstddef>
#include <cstdint>

#include <PixelForge/core/macros.hpp>

export module PixelForge.mem:align;

import PixelForge.core;

namespace pf {

export namespace mem {

[[nodiscard]]
constexpr std::size_t
alignmentPadding(std::uintptr_t ptr, std::size_t alignment) PF_NOEXCEPT {
  PF_REQUIRE(alignment != 0);
  PF_REQUIRE(std::has_single_bit(alignment));
  return (alignment - (ptr & (alignment - 1))) & (alignment - 1);
}

/**@brief Invalid if alignment is not a power of 2 or ptr is nullptr
 */
template <PointerLike_c T_ptr>
  requires(!std::is_same_v<T_ptr, std::nullptr_t>)
[[nodiscard]]
constexpr T_ptr
align(T_ptr ptr, std::size_t alignment) PF_NOEXCEPT {
  PF_REQUIRE_ASSUME(ptr != nullptr, "nullptr is not valid here!");
  PF_REQUIRE(std::has_single_bit(alignment));

  return ptr + alignmentPadding(pointer_cast<std::uintptr_t>(ptr), alignment);
}

}

}
