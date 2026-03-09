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
    : m_data(pBuf), m_top(pBuf), m_end(reinterpret_cast<T*>(data_() + capacity)) {
    PF_REQUIRE(valid_init_());
  }
  
  constexpr
  Stack(std::span<T> buf) PF_NOEXCEPT 
    : m_data(buf.data()), m_top(buf.data()), m_end(reinterpret_cast<T*>(data_() + buf.size())) {
    PF_REQUIRE(valid_init_());
  }

  [[nodiscard]] constexpr const T* 
  data(void) const PF_NOEXCEPT {
    return m_data;
  }

  [[nodiscard]] constexpr T* 
  data(void) PF_NOEXCEPT {
    return m_data;
  }

  [[nodiscard]] constexpr const T*
  end(void) const PF_NOEXCEPT {
    return m_end;
  }

  [[nodiscard]] constexpr T*
  end(void) PF_NOEXCEPT {
    return m_end;
  }

  [[nodiscard]] constexpr size_type
  size(void) const PF_NOEXCEPT {
    return top_() - data_();
  }

  [[nodiscard]] constexpr size_type
  capacity(void) const PF_NOEXCEPT {
    return end_() - data_();
  }

  [[nodiscard]] constexpr size_type
  remaining(void) const PF_NOEXCEPT {
    return end_() - top_();
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

  template<class T_ErrPolicy = ErrPolicy_throws<void, FullError>>
  requires VoidErrPolicy_c<T_ErrPolicy> &&
  requires { { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>; }
  constexpr T_ErrPolicy::return_type
  pop(void) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_v || T_ErrPolicy::is_noexcept) {
    if (empty()) {
      return T_ErrPolicy::fail();
    }
    m_top--;
    T temp = std::move(m_top);
    std::destroy_at(std::addressof(m_top));  

    return T_ErrPolicy::success(std::move(temp));
  }

  /**
   *@brief by default throws \ref Error::Full if full, see \ref emplace_unchecked
  */
  template<class... V_args, class T_ErrPolicy = ErrPolicy_throws<pointer, FullError>>
  requires ErrPolicy_c<T_ErrPolicy, pointer> &&
  requires { { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>; }
  constexpr T_ErrPolicy::return_type
  emplace(V_args... args) PF_NOEXCEPT_COND(Traits::template is_nothrow_construct_v<V_args...> || T_ErrPolicy::is_noexcept) {
    if (full()) {
      return T_ErrPolicy::fail();
    }

    return T_ErrPolicy::success(emplace_impl_<V_args...>(args...));
  }

private:
  T* m_data{nullptr};
  T* m_top{nullptr};
  T* m_end{nullptr};

  [[nodiscard]] constexpr std::uintptr_t
  top_(void) const PF_NOEXCEPT {
    return reinterpret_cast<std::uintptr_t>(m_top);
  }

  [[nodiscard]] constexpr std::uintptr_t
  end_(void) const PF_NOEXCEPT {
    return reinterpret_cast<std::uintptr_t>(m_end);
  }

  [[nodiscard]] constexpr std::uintptr_t
  data_(void) const PF_NOEXCEPT {
    return reinterpret_cast<std::uintptr_t>(m_data);
  }

  [[nodiscard]] constexpr bool
  valid_init_(void) const PF_NOEXCEPT {
    return m_data != nullptr &&
           (data_() % alignof(T)) == 0;
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
