module;

#include <type_traits>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:meta.enumBits; // doesnt like being called .enum

namespace pf {

/**
 *@brief Concept matching scoped enum types (enum class)
 *
 *@tparam T the type to check
 */
export template <typename T>
concept EnumClass_c =
    std::is_enum_v<T> && !std::is_convertible_v<T, std::underlying_type_t<T>>;

/**
 *@brief Wrapper enabling bitwise set operations on scoped enum flags
 *
 * Scoped enums deliberately do not support operator| etc.; Flag restores
 * them for bitmask-style usage while keeping values convertible back to
 * the enum type.
 *
 *@tparam T_Bit_t the scoped enum type whose enumerators act as bits
 */
template <EnumClass_c T_Bit_t>
class Flag {
public:
  /// The enum's underlying integer type
  using underlying_t = std::underlying_type_t<T_Bit_t>;

  /**
   *@brief Wraps a single enumerator as a flag set
   */
  constexpr Flag(T_Bit_t bit) PF_NOEXCEPT : m_val(static_cast<underlying_t>(bit)) {};

  /**
   *@brief The raw underlying bit pattern
   */
  [[nodiscard]]
  constexpr underlying_t
  val() const PF_NOEXCEPT {
    return m_val;
  }

  [[nodiscard]]
  constexpr bool
  operator==(const Flag&) const = default;

  /**
   *@brief Compares against a bare enumerator
   */
  [[nodiscard]]
  constexpr bool
  operator==(T_Bit_t bit) const {
    return m_val == cast(bit);
  }

  /**
   *@brief Intersection of two flag sets
   */
  [[nodiscard]]
  constexpr Flag
  operator&(const Flag& other) const {
    Flag retval = other;
    retval.m_val &= m_val;
    return retval;
  }

  /**
   *@brief In-place intersection with another flag set
   */
  [[nodiscard]]
  constexpr Flag
  operator&=(const Flag& other) const {
    m_val &= other.m_val;
    return *this;
  }

  /**
   *@brief Union of two flag sets
   */
  [[nodiscard]]
  constexpr Flag
  operator|(const Flag& other) const {
    Flag retval = other;
    retval.m_val |= m_val;
    return retval;
  }

  /**
   *@brief In-place union with another flag set
   */
  [[nodiscard]]
  constexpr Flag
  operator|=(const Flag& other) const {
    m_val |= other.m_val;
    return *this;
  }

  /**
   *@brief Union of two enumerators as a flag set
   */
  [[nodiscard]]
  friend constexpr // no implied self
      T_Bit_t
      operator&(T_Bit_t lhs, T_Bit_t rhs) PF_NOEXCEPT {
    return static_cast<T_Bit_t>(cast(lhs) | cast(rhs));
  }

  /**
   *@brief In-place intersection with a bare enumerator
   */
  [[nodiscard]]
  constexpr Flag&
  operator&=(T_Bit_t rhs) PF_NOEXCEPT {
    m_val &= cast(rhs);
    return *this;
  }

  /**
   *@brief Union of two enumerators as a flag set
   */
  [[nodiscard]]
  friend constexpr Flag
  operator|(T_Bit_t lhs, T_Bit_t rhs) PF_NOEXCEPT {
    Flag retval(lhs);
    retval.m_val |= cast(rhs);
    return retval;
  }

  /**
   *@brief In-place union with a bare enumerator
   */
  [[nodiscard]]
  constexpr Flag&
  operator|=(T_Bit_t rhs) PF_NOEXCEPT {
    m_val |= cast(rhs);
    return *this;
  }

private:
  underlying_t m_val;

  [[nodiscard]]
  static constexpr underlying_t
  cast(T_Bit_t val) PF_NOEXCEPT {
    return static_cast<underlying_t>(val);
  }
};

}
