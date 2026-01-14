module;

#include <type_traits>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.meta.enumBits; // doesnt like being called .enum

namespace pf {

export
template<typename T>
concept EnumClass_c = std::is_enum_v<T> && !std::is_convertible_v<T, std::underlying_type_t<T>>;

template<EnumClass_c T_Bit_t>
class Flags {
public:
using underlying_t = std::underlying_type_t<T_Bit_t>;

constexpr
Flags(T_Bit_t bit) PF_NOEXCEPT 
  : m_val(static_cast<underlying_t>(bit)) {};

[[nodiscard]] 
constexpr 
underlying_t
val(void) const PF_NOEXCEPT {
  return m_val;
}

[[nodiscard]]
constexpr
bool operator==(const Flags&) const = default;

[[nodiscard]]
constexpr
bool operator==(T_Bit_t bit) const {
  return m_val == cast(bit);
}

[[nodiscard]]
constexpr
Flags
operator&(const Flags& other) const {
  Flags retval = other;
  retval.m_val &= m_val;
  return retval;
}

[[nodiscard]]
constexpr
Flags
operator&=(const Flags& other) const {
  m_val &= other.m_val;
  return *this;
}

[[nodiscard]]
constexpr
Flags
operator|(const Flags& other) const {
  Flags retval = other;
  retval.m_val |= m_val;
  return retval;
}

[[nodiscard]]
constexpr
Flags
operator|=(const Flags& other) const {
  m_val |= other.m_val;
  return *this;
}

[[nodiscard]] 
friend constexpr // no implied self
T_Bit_t
operator&(T_Bit_t lhs, T_Bit_t rhs) PF_NOEXCEPT {
  return static_cast<T_Bit_t>(cast(lhs) | cast(rhs));
}

[[nodiscard]]
constexpr
Flags& operator&=(T_Bit_t rhs) PF_NOEXCEPT {
  m_val &= cast(rhs);
  return *this;
}

[[nodiscard]]
friend constexpr
Flags
operator|(T_Bit_t lhs, T_Bit_t rhs) PF_NOEXCEPT {
  Flags retval(lhs);
  retval.m_val |= cast(rhs);
  return retval;
}

[[nodiscard]]
constexpr
Flags&
operator|=(T_Bit_t rhs) PF_NOEXCEPT {
  m_val |= cast(rhs);
  return *this;
}

private:
  underlying_t m_val;
  
  [[nodiscard]]
  static constexpr
  underlying_t 
  cast(T_Bit_t val) PF_NOEXCEPT {
    return static_cast<underlying_t>(val);
  }
};

}
