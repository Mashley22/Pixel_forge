module;

#include <concepts>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.math;

import PixelForge.core.require;

namespace pf {

export namespace math {

template<std::unsigned_integral T>
[[nodiscard]]
constexpr bool
isPowerOfTwo(T val) PF_NOEXCEPT {
  return (val != 0) && ((val & (val - 1)) == 0);
}

template<std::unsigned_integral T>
[[nodiscard]] constexpr T
modPow2Value(T x, T pow2Val) PF_NOEXCEPT {
  PF_REQUIRE(isPowerOfTwo(pow2Val));
  return x & (pow2Val - 1);
}

}

}
