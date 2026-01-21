module;

#include <concepts>
#include <string_view>

export module PixelForge.core.meta.svToInt;

import PixelForge.core.utils.charEncoding;

namespace pf {

namespace meta {

// use std::stoll etc. for runtime
// may add overflow checking but should be pretty obvious yourself :/
export
template<
  std::integral T,
  std::size_t T_base = 10,
  const std::string_view& T_digitSet = digitSetUpper
>
[[nodiscard]]
consteval T
svToInt(std::string_view sv) noexcept {
  
  T retVal = 0;
  std::size_t multiplier = 1;
  for (auto it = sv.rbegin(); it != sv.rend() - 1; it++) {
    retVal += charToInt<T_base, T_digitSet>(*it) * multiplier;
    multiplier *= T_base;
  }

  if constexpr (std::is_unsigned_v<T>) {
    if (sv[0] == '-') {
      throw "Signed input with unsigned type!";
    }
    retVal += charToInt<T_base, T_digitSet>(sv[0]) * multiplier;
    return retVal;
  }
  else {
    if (sv[0] == '-') {
      return retVal * -1;
    }
    else {
      retVal += charToInt<T_base, T_digitSet>(sv[0]) * multiplier;
      return retVal;
    }
  }
}

}

}
