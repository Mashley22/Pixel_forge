module;

#include <string_view>

export module PixelForge.core.utils.charEncoding;

import PixelForge.core.exception;

namespace pf {

export
class InvalidCharToHexError : public Exception {
public:
  InvalidCharToHexError(char chr);
};

export 
class InvalidHexToCharError : public Exception {
public:
  InvalidHexToCharError(int val);
};

export
constexpr std::string_view digitSetUpper = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

export 
constexpr std::string_view digitSetLower = "0123456789abcdefghijklmnopqrstuvwxyz";

export 
template<
  std::size_t T_base = 10,
  const std::string_view& T_digitSet = digitSetUpper
>
[[nodiscard]]
constexpr
int
charToInt(char chr) {
  static_assert(T_base <= T_digitSet.size());
  std::size_t pos = T_digitSet.find(chr);

  if (pos == std::string_view::npos) {
    throw InvalidCharToHexError(chr);
  }
  
  return static_cast<int>(pos);
}

export
template<
  std::size_t T_base = 10,
  const std::string_view& T_digitSet = digitSetUpper
>
[[nodiscard]]
constexpr 
char
intToChar(int value) {
  if (value < 0 || value >= T_base) {
    throw InvalidHexToCharError(value);
  }

  return T_digitSet[value];
}

}
