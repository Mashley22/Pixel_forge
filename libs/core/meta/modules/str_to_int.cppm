module;

#include <concepts>
#include <string_view>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:meta.strToInt;

import :utils.charEncoding;

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
 * character must exist in @p T_digitSet or compilation fails via
 * InvalidCharToHexError.
 *
 *@tparam T integral result type
 *@tparam T_base numeric base the string is written in
 *@tparam T_digitSet character set mapping glyphs to digit values
 *
 *@param sv the compile time string to parse
 *
 *@return the parsed value
 */
export template <std::integral T,
                 std::size_t T_base = 10,
                 const std::string_view& T_digitSet = digitSetUpper>
[[nodiscard]]
consteval T
strToInt(std::string_view sv)
    PF_NOEXCEPT { // NOLINT, you gotta be a dumbass for this to throw

  T retVal = 0;
  std::size_t multiplier = 1;

  auto retValIncrement = [](char val, std::size_t mult) {
    return static_cast<T>(static_cast<std::size_t>(charToInt<T_base, T_digitSet>(val)) *
                          mult);
  };

  for (auto it = sv.rbegin(); it != sv.rend() - 1; it++) {
    retVal += retValIncrement(*it, multiplier);
    multiplier *= T_base;
  }

  if constexpr (std::is_unsigned_v<T>) {
    if (sv[0] == '-') {
      throw "Signed input with unsigned type!";
    }
    retVal += retValIncrement(sv[0], multiplier);
    return retVal;
  } else {
    if (sv[0] == '-') {
      return retVal * -1;
    } else {
      retVal += retValIncrement(sv[0], multiplier);
      return retVal;
    }
  }
}

}

}
