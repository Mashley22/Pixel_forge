module;

#include <concepts>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:math;

import :require;

namespace pf {

export namespace math {

/**
 *@brief Checks whether a value is an exact power of two
 *
 *@tparam T unsigned integral type
 *@param val value to inspect
 *
 *@return true if @p val is a non-zero power of two, false otherwise
 * (including zero)
 */
template <std::unsigned_integral T>
[[nodiscard]]
constexpr bool
isPowerOfTwo(T val) PF_NOEXCEPT {
  return (val != 0) && ((val & (val - 1)) == 0);
}

/**
 *@brief Computes @p x modulo a power of two via bitmasking
 *
 * Equivalent to @p x % @p pow2Val but without division; the modulus is
 * enforced to be a power of two.
 *
 *@tparam T unsigned integral type
 *@param x value to reduce
 *@param pow2Val modulus, must be a power of two (checked by pf::require)
 *
 *@return @p x modulo @p pow2Val
 */
template <std::unsigned_integral T>
[[nodiscard]] constexpr T
modPow2Value(T x, T pow2Val) PF_NOEXCEPT {
  PF_REQUIRE(isPowerOfTwo(pow2Val));
  return x & (pow2Val - 1);
}

}

}
