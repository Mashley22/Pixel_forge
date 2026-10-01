module;

#include <string_view>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:utils.charEncoding;

import :errors.exception;

#define DEFAULT_BASE 10

namespace pf {

/**
 *@brief Error thrown by char_to_int() when a glyph is not part of the digit
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
  input_char() const PF_NOEXCEPT {
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
  input_val() const PF_NOEXCEPT {
    return m_val;
  }

private:
  const int m_val;
};

/**
 *@brief Upper case digit glyphs for bases up to 36
 */
export constexpr std::string_view digit_set_upper =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

/**
 *@brief Lower case digit glyphs for bases up to 36
 */
export constexpr std::string_view digit_set_lower =
    "0123456789abcdefghijklmnopqrstuvwxyz";

/**
 *@brief Converts a single glyph into its digit value
 *
 *@tparam Base numeric base, must not exceed DigitSet.size() (checked at
 * compile time)
 *@tparam DigitSet character set mapping glyphs to digit values
 *
 *@param chr glyph to convert, e.g. 'F'
 *
 *@return the digit value of @p chr, e.g. 15
 *
 *@throw InvalidCharToHexError if @p chr is not in @p DigitSet
 */
export template <std::size_t BaseT = DEFAULT_BASE,
                 const std::string_view& TDigitSet = digit_set_upper>
[[nodiscard]] constexpr int
char_to_int(char chr) {
  static_assert(BaseT <= TDigitSet.size());
  std::size_t pos = TDigitSet.find(chr);

  if (pos == std::string_view::npos) {
    throw InvalidCharToHexError(chr);
  }

  return static_cast<int>(pos);
}

/**
 *@brief Converts a digit value into its glyph
 *
 *@tparam Base numeric base, values outside [0, Base) are rejected
 *@tparam DigitSet character set mapping digit values to glyphs
 *
 *@param value digit value to convert, e.g. 15
 *
 *@return the glyph for @p value in @p DigitSet, e.g. 'F'
 *
 *@throw InvalidHexToCharError if @p value is negative or >= Base
 */
export template <std::size_t BaseT = DEFAULT_BASE,
                 const std::string_view& TDigitSet = digit_set_upper>
[[nodiscard]] constexpr char
int_to_char(int value) {
  if (value < 0 || value >= BaseT) {
    throw InvalidHexToCharError(value);
  }

  return TDigitSet[value];
}

}
