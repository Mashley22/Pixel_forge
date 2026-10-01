module;

#include <concepts>
#include <string_view>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:meta.str_to_int;

import :utils.charEncoding;

#define DEFAULT_BASE 10

namespace pf {

namespace meta {

// use std::stoll etc. for runtime
// may add overflow checking but should be pretty obvious yourself :/

/**
 *@brief Parses a compile time string into an integral value in an arbitrary
 * base
 *
 * Digits are consumed from the right, so the string must not be empty and
 * only a single leading '-' is understood (signed types only). Any other
 * character must exist in @p DigitSet or compilation fails via
 * InvalidCharToHexError.
 *
 *@tparam T integral result type
 *@tparam Base numeric base the string is written in
 *@tparam DigitSet character set mapping glyphs to digit values
 *
 *@param sv the compile time string to parse
 *
 *@return the parsed value
 */
export template <std::integral T,
                 std::size_t BaseT = DEFAULT_BASE,
                 const std::string_view& TDigitSet = digit_set_upper>
[[nodiscard]]
consteval T
str_to_int(std::string_view sv)
    PF_NOEXCEPT { // NOLINT, you gotta be a dumbass for this to throw

  T ret_val = 0;
  std::size_t multiplier = 1;

  auto ret_val_increment = [](char val, std::size_t mult) {
    return static_cast<T>(static_cast<std::size_t>(char_to_int<BaseT, TDigitSet>(val)) *
                          mult);
  };

  for (auto it = sv.rbegin(); it != sv.rend() - 1; it++) {
    ret_val += ret_val_increment(*it, multiplier);
    multiplier *= BaseT;
  }

  if constexpr (std::is_unsigned_v<T>) {
    if (sv[0] == '-') {
      throw "Signed input with unsigned type!";
    }
    ret_val += ret_val_increment(sv[0], multiplier);
    return ret_val;
  } else {
    if (sv[0] == '-') {
      return ret_val * -1;
    } else {
      ret_val += ret_val_increment(sv[0], multiplier);
      return ret_val;
    }
  }
}

}

}
