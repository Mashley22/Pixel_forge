module;

#include <string_view>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:utils.charEncoding;

import :errors.exception;

namespace pf {

/**
 *@brief Error thrown by charToInt() when a glyph is not part of the digit
 * set
 */
export class InvalidCharToHexError : public Exception {
public:
  static constexpr std::string_view what_arg = "Invalid char to hex";

  /**
   *@brief Remembers @p chr so callers can inspect the offending input
   */
  constexpr InvalidCharToHexError(char chr) PF_NOEXCEPT : Exception(what_arg),
                                                          m_chr(chr) {}

  /**
   *@brief The character that could not be converted
   */
  [[nodiscard]] constexpr char
  inputChar() const PF_NOEXCEPT {
    return m_chr;
  }

private:
  const char m_chr;
};

/**
 *@brief Error thrown by intToChar() when a value has no glyph in the digit
 * set
 */
export class InvalidHexToCharError : public Exception {
public:
  static constexpr std::string_view what_arg = "Invalid hex to char";

  /**
   *@brief Remembers @p val so callers can inspect the offending input
   */
  constexpr InvalidHexToCharError(int val) PF_NOEXCEPT : Exception(what_arg),
                                                         m_val(val) {}

  /**
   *@brief The value that could not be converted
   */
  [[nodiscard]] constexpr int
  inputVal() const PF_NOEXCEPT {
    return m_val;
  }

private:
  const int m_val;
};

/**
 *@brief Upper case digit glyphs for bases up to 36
 */
export constexpr std::string_view digitSetUpper = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

/**
 *@brief Lower case digit glyphs for bases up to 36
 */
export constexpr std::string_view digitSetLower = "0123456789abcdefghijklmnopqrstuvwxyz";

/**
 *@brief Converts a single glyph into its digit value
 *
 *@tparam T_base numeric base, must not exceed T_digitSet.size() (checked at
 * compile time)
 *@tparam T_digitSet character set mapping glyphs to digit values
 *
 *@param chr glyph to convert, e.g. 'F'
 *
 *@return the digit value of @p chr, e.g. 15
 *
 *@throw InvalidCharToHexError if @p chr is not in @p T_digitSet
 */
export template <std::size_t T_base = 10,
                 const std::string_view& T_digitSet = digitSetUpper>
[[nodiscard]] constexpr int
charToInt(char chr) {
  static_assert(T_base <= T_digitSet.size());
  std::size_t pos = T_digitSet.find(chr);

  if (pos == std::string_view::npos) {
    throw InvalidCharToHexError(chr);
  }

  return static_cast<int>(pos);
}

/**
 *@brief Converts a digit value into its glyph
 *
 *@tparam T_base numeric base, values outside [0, T_base) are rejected
 *@tparam T_digitSet character set mapping digit values to glyphs
 *
 *@param value digit value to convert, e.g. 15
 *
 *@return the glyph for @p value in @p T_digitSet, e.g. 'F'
 *
 *@throw InvalidHexToCharError if @p value is negative or >= T_base
 */
export template <std::size_t T_base = 10,
                 const std::string_view& T_digitSet = digitSetUpper>
[[nodiscard]] constexpr char
intToChar(int value) {
  if (value < 0 || value >= T_base) {
    throw InvalidHexToCharError(value);
  }

  return T_digitSet[value];
}

}
