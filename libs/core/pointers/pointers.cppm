module;

#include <bit>
#include <cstdint>
#include <type_traits>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:pointers;

import :require;

export namespace pf {

template <typename T_ptr>
concept PointerLike_c =
    std::is_pointer_v<T_ptr> || std::is_same_v<std::uintptr_t, T_ptr> ||
    std::is_same_v<std::intptr_t, T_ptr> || std::is_same_v<std::ptrdiff_t, T_ptr>;

template <PointerLike_c T_to, PointerLike_c T_from>
[[nodiscard]] constexpr T_to
pointer_cast(T_from ptr) PF_NOEXCEPT {
  if (std::is_constant_evaluated()) {
    return std::bit_cast<T_to>(ptr); // NOLINT(bugprone-bitwise-pointer-cast)
  }
  return reinterpret_cast<T_to>(ptr);
}

template <typename T>
[[nodiscard]] constexpr bool
isAligned(void* ptr) PF_NOEXCEPT {
  return (pointer_cast<std::uintptr_t>(ptr) % alignof(T)) == 0;
}

template <typename T>
[[nodiscard]] constexpr bool
isAligned(std::byte* ptr) PF_NOEXCEPT {
  return isAligned<T>(pointer_cast<void*>(ptr));
}

template <typename T>
[[nodiscard]] constexpr bool
isAligned(char* ptr) PF_NOEXCEPT {
  return isAligned<T>(pointer_cast<void*>(ptr));
}

template <typename T>
[[nodiscard]] constexpr bool
isAligned(unsigned char* ptr) PF_NOEXCEPT {
  return isAligned<T>(pointer_cast<void*>(ptr));
}

template <typename T>
concept NullptrComparable_c = requires(T val) {
  { val != nullptr } -> std::same_as<bool>;
  { val == nullptr } -> std::same_as<bool>;
};

/**
 * @brief A non-null pointer wrapper
 *
 * NonNull<T> wraps a pointer type and guarantees the pointer is never null
 * at the API boundary. The constructor enforces this via the require system.
 *
 * - Implicit conversion to the underlying pointer type (T)
 * - Explicit construction from pointer (cannot accidentally create one)
 * - Zero-overhead: same size/alignment as T
 *
 * @tparam T The pointer type
 */
template <typename T>
  requires PointerLike_c<T> && NullptrComparable_c<T>
class NonNull {
public:
  using pointer_type = T;
  using element_type = std::remove_pointer_t<T>;

  /**
   * @brief Construct from a pointer - explicit to prevent accidental creation
   */
  constexpr explicit NonNull(T ptr) PF_NOEXCEPT : m_ptr(ptr) {
    PF_REQUIRE(ptr != nullptr, "NonNull constructed from null pointer");
  }

  NonNull() = delete;
  constexpr NonNull(const NonNull&) PF_NOEXCEPT = default;
  constexpr NonNull(NonNull&&) PF_NOEXCEPT = default;
  constexpr NonNull&
  operator=(const NonNull&) PF_NOEXCEPT = default;
  constexpr NonNull&
  operator=(NonNull&&) PF_NOEXCEPT = default;
  ~NonNull() = default;

  /**
   * @brief Implicit conversion to underlying pointer type
   */
  [[nodiscard]] constexpr
  operator T() const PF_NOEXCEPT {
    [[assume(m_ptr != nullptr)]];
    return m_ptr;
  }

  /**
   * @brief Dereference
   */
  [[nodiscard]] constexpr element_type&
  operator*() const PF_NOEXCEPT {
    [[assume(m_ptr != nullptr)]];
    return *m_ptr;
  }

  /**
   * @brief Member access
   */
  [[nodiscard]] constexpr element_type*
  operator->() const PF_NOEXCEPT {
    [[assume(m_ptr != nullptr)]];
    return m_ptr;
  }

  /**
   * @brief Explicit cast from pointer to NonNull
   */
  [[nodiscard]] static constexpr NonNull
  from(T ptr) PF_NOEXCEPT {
    return NonNull(ptr);
  }

private:
  T m_ptr;
};

/**
 * @brief Deduction guide for raw pointers
 */
template <typename T>
NonNull(T*) -> NonNull<T*>;

} // namespace pf
