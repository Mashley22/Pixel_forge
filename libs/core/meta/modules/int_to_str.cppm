module;

#include <concepts>
#include <string_view>

export module PixelForge.core:meta.intToStr;

import :utils.charEncoding;

namespace pf {

namespace meta {

// can also use the IntToSv, this is just a very natural stopping off point in
// the implementation might make this a private impl ...

/**
 *@brief Converts an unsigned compile time value into its string
 * representation in an arbitrary base
 *
 * All work happens at compile time; access the result via sv(), str(),
 * c_str() or arr.
 *
 *@tparam T unsigned integral type of @p T_val
 *@tparam T_val the value to convert
 *@tparam T_base numeric base, 2 <= T_base <= T_digitSet.size()
 *@tparam T_digitSet character set mapping digit values to glyphs
 */
export template <std::unsigned_integral T,
                 T T_val,
                 std::size_t T_base = 10,
                 const std::string_view& T_digitSet = digitSetUpper>
struct UintToStr {
private:
  /**
   *@brief Number of digits in the output
   */
  [[nodiscard]]
  static consteval std::size_t
  digits() {
    std::size_t digitCount = 1;
    T val = T_val;

    while (val >= T_base) {
      val /= T_base;
      digitCount++;
    }

    return digitCount;
  }

  [[nodiscard]]
  static consteval std::size_t
  len() {
    return digits() + 1;
  }

  [[nodiscard]]
  static consteval T
  maxBaseScale() {
    T scale = 1;
    for (std::size_t i = 1; i < digits(); i++) {
      scale *= T_base;
    }

    return scale;
  }

  [[nodiscard]]
  static consteval std::array<char, len()>
  impl() {
    std::array<char, len()> retVal{};
    T remaining = T_val;
    T scale = maxBaseScale();

    for (std::size_t i = 0; i < digits(); i++) {
      retVal[i] = T_digitSet[remaining / scale];
      remaining %= scale;
      scale /= T_base;
    }

    retVal.back() = '\0';

    return retVal;
  }

public:
  /// Null-terminated compile time storage of the converted value
  static constexpr std::array<char, len()> arr = impl();

  /**
   *@brief The converted value as a string, without the null terminator
   */
  [[nodiscard]]
  static consteval std::string_view
  sv() {
    return {arr.data(), arr.size() - 1};
  }

  /**
   *@brief The converted value as a null-terminated C string
   */
  [[nodiscard]]
  static consteval const char*
  c_str() {
    return arr.data();
  }

  /**
   *@brief Alias of sv()
   */
  [[nodiscard]]
  static consteval std::string_view
  str() {
    return {arr.data(), arr.size() - 1};
  }
};

/**
 *@brief Converts a signed compile time value into its decimal (or custom
 * base) string representation, prefixing a minus sign for negative values
 *
 * Negative values are converted through their unsigned counterpart before
 * rendering.
 *
 *@tparam T signed integral type of @p T_val
 *@tparam T_val the value to convert
 *@tparam T_base numeric base
 *@tparam T_digitSet character set mapping digit values to glyphs
 */
export template <std::signed_integral T,
                 T T_val,
                 std::size_t T_base = 10,
                 const std::string_view& T_digitSet = digitSetUpper>
struct IntToStr {
private:
  using unsigned_t = std::make_unsigned_t<T>;

  /**
   *@brief Magnitude of T_val computed without signed overflow, so even the
   * most negative value converts correctly
   */
  static constexpr unsigned_t magnitude =
      (T_val < 0) ? static_cast<unsigned_t>(-static_cast<unsigned_t>(T_val))
                  : static_cast<unsigned_t>(T_val);

  static constexpr auto UintArr =
      UintToStr<unsigned_t, magnitude, T_base, T_digitSet>::arr;

  [[nodiscard]]
  static consteval std::size_t
  len() {
    if constexpr (T_val >= 0) {
      return UintArr.size();
    }
    return UintArr.size() + 1;
  }

  [[nodiscard]]
  static consteval std::array<char, len()>
  impl() {
    std::array<char, len()> retVal;
    if constexpr (T_val >= 0) {
      std::copy(UintArr.begin(), UintArr.end(), retVal.begin());
      return retVal;
    }

    retVal[0] = '-';
    std::copy(UintArr.begin(), UintArr.end(), retVal.begin() + 1);
    return retVal;
  }

public:
  /// Null-terminated compile time storage of the converted value
  static constexpr auto arr = impl();

  /**
   *@brief The converted value as a string, without the null terminator
   */
  [[nodiscard]]
  static consteval std::string_view
  sv() {
    return {arr.data(), arr.size() - 1};
  }

  /**
   *@brief The converted value as a null-terminated C string
   */
  [[nodiscard]]
  static consteval const char*
  c_str() {
    return arr.data();
  }
};

}

}
