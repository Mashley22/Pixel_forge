module;

#include <concepts>
#include <bit>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.math;

namespace pf {

export namespace math {

template<std::unsigned_integral T>
[[nodiscard]]
constexpr bool
isPowerOfTwo(T val) PF_NOEXCEPT {
  return std::has_single_bit(val);
}

}

}
