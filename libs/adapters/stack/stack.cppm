module;

#include <cstdint>
#include <type_traits>
#include <concepts>
#include <span>
#include <optional>

#include <PixelForge/core/macros.hpp>
#include <PixelForge/adapters/macros.hpp>

export module PixelForge.adapters.stack;

import PixelForge.adapters.utils.traits;

import PixelForge.core;

#define NOEXCEPT_MOVE PF_NOEXCEPT_COND(Traits::is_nothrow_move_v)
#define NOEXCEPT_COPY PF_NOEXCEPT_COND(Traits::is_nothrow_copy_v)
#define NOEXCEPT_CONSTRUCT(...) PF_NOEXCEPT_COND(Traits::template is_nothrow_construct_v<__VA_ARGS__>)

export namespace pf::adapters {

template<class T>
class Stack {
public:
  struct Error : public Exception {};

  struct FullError : public Error {};
  struct EmptyError : public Error {};

  struct Traits {
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = T*;
    using const_pointer = const pointer;

    static constexpr bool is_nothrow_copy_v = std::is_nothrow_copy_constructible_v<T>;
    static constexpr bool is_nothrow_move_v = std::is_nothrow_move_constructible_v<T>;
    template<typename... V_args>
    static constexpr bool is_nothrow_construct_v = std::is_nothrow_constructible_v<T, V_args...>;
  };

  PF_ADAPTERS_INHERIT_TRAITS(Traits);

  constexpr Stack(void) PF_NOEXCEPT = default;

  constexpr
  Stack(T* pBuf, size_type capacity) PF_NOEXCEPT 
    : m_data(pBuf), m_top(pBuf), m_end(pBuf + capacity) {
    PF_REQUIRE(valid_init_());
  }
  
  constexpr
  Stack(std::span<T> buf) PF_NOEXCEPT 
    : m_data(buf.data()), m_top(buf.data()), m_end(buf.data() + buf.size()) {
    PF_REQUIRE(valid_init_());
  }

  [[nodiscard]] constexpr const T* 
  data(void) const PF_NOEXCEPT {
    return m_data;
  }

  [[nodiscard]] constexpr const T*
  end(void) const PF_NOEXCEPT {
    return m_end;
  }

  [[nodiscard]] constexpr size_type
  size(void) const PF_NOEXCEPT {
    [[assume(m_top >= m_data)]];
    return static_cast<size_type>(m_top - m_data);
  }

  [[nodiscard]] constexpr size_type
  capacity(void) const PF_NOEXCEPT {
    [[assume(m_end > m_data)]];
    return static_cast<size_type>(m_end - m_data);
  }

  [[nodiscard]] constexpr size_type
  remaining(void) const PF_NOEXCEPT {
    [[assume(m_end >= m_top)]];
    return static_cast<size_type>(m_end - m_top);
  }

  [[nodiscard]] constexpr bool
  full(void) const PF_NOEXCEPT {
    return size() == capacity();
  }

  [[nodiscard]] constexpr bool
  empty(void) const PF_NOEXCEPT {
    return size() == 0;
  }

  template<class T_ErrPolicy = ErrPolicy_throws<void, FullError>>
  requires VoidErrPolicy_c<T_ErrPolicy> &&
  requires { { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>; }
  constexpr T_ErrPolicy::return_type
  push(const T& value) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_v || T_ErrPolicy::is_noexcept) {
    if (full()) {
      return T_ErrPolicy::fail();
    }

    *m_top= value;
    m_top++;
    return T_ErrPolicy::success();
  }

  constexpr ErrPolicy_optional<void>::return_type
  try_push(const T& value) NOEXCEPT_MOVE {
    return push<ErrPolicy_optional<void>>(value);
  }

  constexpr ErrPolicy_nothing<void>::return_type
  push_unchecked(const T& value) NOEXCEPT_MOVE {
    return push<ErrPolicy_nothing<void>>(value);
  }

  template<class T_ErrPolicy = ErrPolicy_throws<void, FullError>>
  requires VoidErrPolicy_c<T_ErrPolicy> &&
  requires { { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>; }
  constexpr T_ErrPolicy::return_type
  push(T&& value) PF_NOEXCEPT_COND(Traits::is_nothrow_move_v || T_ErrPolicy::is_noexcept) {
    if(full()) {
      return T_ErrPolicy::fail();
    }

    emplace_unchecked(std::forward<T>(value));
    return T_ErrPolicy::success();
  }

  constexpr ErrPolicy_nothing<void>::return_type
  push_unchecked(T&& value) PF_NOEXCEPT_COND(Traits::is_nothrow_move_v) {
    push<ErrPolicy_nothing<void>>(std::forward<T>(value));
  }

  constexpr ErrPolicy_optional<void>::return_type
  try_push(T&& value) PF_NOEXCEPT_COND(Traits::is_nothrow_move_v) {
    return push<ErrPolicy_optional<void>>(std::forward<T>(value));
  }

  template<class T_ErrPolicy = ErrPolicy_throws<T, FullError>>
  requires ErrPolicy_c<T_ErrPolicy, T> &&
  requires { { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>; }
  constexpr T_ErrPolicy::return_type
  pop(void) PF_NOEXCEPT_COND(Traits::is_nothrow_move_v || T_ErrPolicy::is_noexcept) {
    if (empty()) {
      return T_ErrPolicy::fail();
    }
    m_top--;
    T temp = std::move(*m_top);
    std::destroy_at(std::addressof(m_top));  

    return T_ErrPolicy::success(std::move(temp));
  }

  constexpr ErrPolicy_optional<T>::return_type
  try_pop(void) NOEXCEPT_MOVE {
    return pop<ErrPolicy_optional<T>>();
  }

  constexpr ErrPolicy_nothing<T>::return_type
  pop_unchecked(void) NOEXCEPT_MOVE {
    return pop<ErrPolicy_nothing<T>>();
  }

  template<class T_ErrPolicy, class... V_args>
  requires ErrPolicy_c<T_ErrPolicy, pointer> &&
  requires { { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>; }
  constexpr T_ErrPolicy::return_type
  emplace(V_args... args) PF_NOEXCEPT_COND(Traits::template is_nothrow_construct_v<V_args...> || T_ErrPolicy::is_noexcept) {
    if (full()) {
      return T_ErrPolicy::fail();
    }

    return T_ErrPolicy::success(emplace_impl_<V_args...>(args...));
  }

  template<class... V_args>
  constexpr ErrPolicy_throws<pointer, FullError>::return_type
  emplace(V_args... args) {
    return emplace<ErrPolicy_throws<pointer, FullError>>(args...);
  }

  template<class... V_args>
  constexpr ErrPolicy_optional<pointer>::return_type
  try_emplace(V_args... args) NOEXCEPT_CONSTRUCT(V_args...) {
    return emplace<ErrPolicy_optional<pointer>>(args...);
  }

  template<class... V_args>
  constexpr ErrPolicy_nothing<pointer>::return_type
  emplace_unchecked(V_args... args) NOEXCEPT_CONSTRUCT(V_args...) {
    return emplace<ErrPolicy_nothing<pointer>>(args...);
  }

private:
  T* m_data{nullptr};
  T* m_top{nullptr};
  T* m_end{nullptr};

  [[nodiscard]] constexpr bool
  valid_init_(void) const PF_NOEXCEPT {
    return m_data != nullptr &&
           (reinterpret_cast<std::uintptr_t>(m_data) % alignof(T)) == 0;
  }

  template<class... V_args>
  pointer
  emplace_impl_(V_args... args) NOEXCEPT_CONSTRUCT(V_args...) {
    PF_REQUIRE(!full());

    new(m_top) T(std::forward<V_args>(args)...);
    pointer retVal = m_top;
    m_top++;

    return retVal;
  }
};

}
