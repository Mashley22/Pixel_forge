module;

#include <concepts>
#include <string_view>

export module PixelForge.core:meta.intToStr;

import :utils.charEncoding;

#define DEFAULT_BASE 10

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
 *@tparam Base numeric base, 2 <= Base <= DigitSet.size()
 *@tparam DigitSet character set mapping digit values to glyphs
 */
export template <std::unsigned_integral T,
                 T ValT,
                 std::size_t BaseT = DEFAULT_BASE,
                 const std::string_view& TDigitSet = digit_set_upper>
struct UintToStr {
private:
  /**
   *@brief Number of digits in the output
   */
  [[nodiscard]]
  static consteval std::size_t
  digits() {
    std::size_t digit_count = 1;
    T val = ValT;

    while (val >= BaseT) {
      val /= BaseT;
      digit_count++;
    }

    return digit_count;
  }

  [[nodiscard]]
  static consteval std::size_t
  len() {
    return digits() + 1;
  }

  [[nodiscard]]
  static consteval T
  max_base_scale() {
    T scale = 1;
    for (std::size_t i = 1; i < digits(); i++) {
      scale *= BaseT;
    }

    return scale;
  }

  [[nodiscard]]
  static consteval std::array<char, len()>
  impl() {
    std::array<char, len()> ret_val{};
    T remaining = ValT;
    T scale = max_base_scale();

    for (std::size_t i = 0; i < digits(); i++) {
      ret_val[i] = TDigitSet[remaining / scale];
      remaining %= scale;
      scale /= BaseT;
    }

    ret_val.back() = '\0';

    return ret_val;
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
 *@tparam Base numeric base
 *@tparam DigitSet character set mapping digit values to glyphs
 */
export template <std::signed_integral T,
                 T ValT,
                 std::size_t BaseT = DEFAULT_BASE,
                 const std::string_view& TDigitSet = digit_set_upper>
struct IntToStr {
private:
  using UnsignedT = std::make_unsigned_t<T>;

  /**
   *@brief Magnitude of T_val computed without signed overflow, so even the
   * most negative value converts correctly
   */
  static constexpr UnsignedT magnitude =
      (ValT < 0) ? static_cast<UnsignedT>(-static_cast<UnsignedT>(ValT))
                 : static_cast<UnsignedT>(ValT);

  static constexpr auto uint_arr = UintToStr<UnsignedT, magnitude, BaseT, TDigitSet>::arr;

  [[nodiscard]]
  static consteval std::size_t
  len() {
    if constexpr (ValT >= 0) {
      return uint_arr.size();
    }
    return uint_arr.size() + 1;
  }

  [[nodiscard]]
  static consteval std::array<char, len()>
  impl() {
    std::array<char, len()> ret_val;
    if constexpr (ValT >= 0) {
      std::copy(uint_arr.begin(), uint_arr.end(), ret_val.begin());
      return ret_val;
    }

    ret_val[0] = '-';
    std::copy(uint_arr.begin(), uint_arr.end(), ret_val.begin() + 1);
    return ret_val;
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
