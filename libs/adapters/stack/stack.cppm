module;

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>

#include <PixelForge/adapters/macros.hpp>
#include <PixelForge/core/macros.hpp>

export module PixelForge.adapters:stack;

import :utils.traits;

import PixelForge.core;

export namespace pf::adapters {

template <class T>
class Stack {
public:
  struct Error : public Exception {
    Error() : Exception("Stack error") {}
  };

  struct FullError : public Error {};
  struct EmptyError : public Error {};

  struct Traits {
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = T*;
    using const_pointer = const T*;

    static constexpr bool is_nothrow_copy_construct_v =
        std::is_nothrow_copy_constructible_v<T>;
    static constexpr bool is_nothrow_move_construct_v =
        std::is_nothrow_move_constructible_v<T>;
    template <typename... V_args>
    static constexpr bool is_nothrow_construct_v =
        std::is_nothrow_constructible_v<T, V_args...>;
  };

  PF_ADAPTERS_INHERIT_TRAITS(Traits);

  constexpr Stack() PF_NOEXCEPT = default;

  /**
   *@brief Constructs a stack over raw storage
   *
   * Elements are constructed in-place over the storage, so it must not hold
   * any live objects; typed buffers are therefore rejected (see the deleted
   * overload below). Storage must be aligned for @p T and outlive the stack
   */
  explicit constexpr Stack(void* pBuf, size_type capacity) PF_NOEXCEPT
    : m_data(static_cast<T*>(pBuf)),
      m_top(m_data),
      m_end(m_data + capacity) {
    PF_REQUIRE(valid_init_());
  }

  /**@overload */
  explicit constexpr Stack(char* pBuf, size_type capacity) PF_NOEXCEPT
    : Stack(static_cast<void*>(pBuf), capacity) {}

  /**@overload */
  explicit constexpr Stack(std::byte* pBuf, size_type capacity) PF_NOEXCEPT
    : Stack(static_cast<void*>(pBuf), capacity) {}

  /**
   *@brief Constructs a stack over a byte span
   *
   * The element capacity is derived as @p buf.size() / sizeof(T); the span
   * must cover a whole number of elements and be aligned for @p T
   */
  explicit constexpr Stack(std::span<char> buf) PF_NOEXCEPT
    : m_data(reinterpret_cast<T*>(buf.data())),
      m_top(m_data),
      m_end(m_data + (buf.size() / sizeof(T))) {
    PF_REQUIRE((buf.size() % sizeof(T)) == 0);
    PF_REQUIRE(valid_init_());
  }

  /**@overload */
  explicit constexpr Stack(std::span<std::byte> buf) PF_NOEXCEPT
    : m_data(reinterpret_cast<T*>(buf.data())),
      m_top(m_data),
      m_end(m_data + (buf.size() / sizeof(T))) {
    PF_REQUIRE((buf.size() % sizeof(T)) == 0);
    PF_REQUIRE(valid_init_());
  }

  constexpr ~Stack() PF_NOEXCEPT { clear(); }

  Stack(const Stack&) = delete;
  Stack(Stack&&) = delete;
  Stack&
  operator=(const Stack&) = delete;
  Stack&
  operator=(Stack&&) = delete;

  [[nodiscard]] constexpr pointer
  data() PF_NOEXCEPT {
    return m_data;
  }

  [[nodiscard]] constexpr const_pointer
  data() const PF_NOEXCEPT {
    return m_data;
  }

  [[nodiscard]] constexpr pointer
  end() PF_NOEXCEPT {
    return m_end;
  }

  [[nodiscard]] constexpr const_pointer
  end() const PF_NOEXCEPT {
    return m_end;
  }

  [[nodiscard]] constexpr size_type
  size() const PF_NOEXCEPT {
    [[assume(m_top >= m_data)]];
    return static_cast<size_type>(m_top - m_data);
  }

  [[nodiscard]] constexpr size_type
  capacity() const PF_NOEXCEPT {
    [[assume(m_end > m_data)]];
    return static_cast<size_type>(m_end - m_data);
  }

  [[nodiscard]] constexpr size_type
  remaining() const PF_NOEXCEPT {
    [[assume(m_end >= m_top)]];
    return static_cast<size_type>(m_end - m_top);
  }

  [[nodiscard]] constexpr bool
  full() const PF_NOEXCEPT {
    return size() == capacity();
  }

  [[nodiscard]] constexpr bool
  empty() const PF_NOEXCEPT {
    return size() == 0;
  }

  [[nodiscard]] constexpr reference
  top() PF_NOEXCEPT {
    PF_REQUIRE(!empty(), "stack empty");
    return *(m_top - 1);
  }

  [[nodiscard]] constexpr const_reference
  top() const PF_NOEXCEPT {
    PF_REQUIRE(!empty(), "stack empty");
    return *(m_top - 1);
  }

  template <class T_ErrPolicy = ErrPolicy_throws<void, FullError>>
    requires VoidErrPolicy_c<T_ErrPolicy> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
  constexpr T_ErrPolicy::return_type
  push(const T& value)
      PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v&& T_ErrPolicy::is_noexcept) {
    if constexpr (T_ErrPolicy::enabled) {
      if (full()) {
        return T_ErrPolicy::fail();
      }
    }

    new (m_top) T(value);
    m_top++;
    return T_ErrPolicy::success();
  }

  constexpr ErrPolicy_optional<void>::return_type
  try_push(const T& value) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    return push<ErrPolicy_optional<void>>(value);
  }

  constexpr void
  push_unchecked(const T& value) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    static constexpr std::string_view funcInfo{PF_FUNC_INFO};
    return push<ErrPolicy_nothing<void, funcInfo>>(value);
  }

  template <class T_ErrPolicy = ErrPolicy_throws<void, FullError>>
    requires VoidErrPolicy_c<T_ErrPolicy> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
  constexpr T_ErrPolicy::return_type
  push(T&& value)
      PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v&& T_ErrPolicy::is_noexcept) {
    if constexpr (T_ErrPolicy::enabled) {
      if (full()) {
        return T_ErrPolicy::fail();
      }
    }

    emplace_unchecked(std::forward<T>(value));
    return T_ErrPolicy::success();
  }

  constexpr void
  push_unchecked(T&& value) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    static constexpr std::string_view funcInfo{PF_FUNC_INFO};
    return push<ErrPolicy_nothing<void, funcInfo>>(std::forward<T>(value));
  }

  constexpr ErrPolicy_optional<void>::return_type
  try_push(T&& value) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    return push<ErrPolicy_optional<void>>(std::forward<T>(value));
  }

  template <class T_ErrPolicy = ErrPolicy_throws<T, FullError>>
    requires ErrPolicy_c<T_ErrPolicy, T> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
  constexpr T_ErrPolicy::return_type
  pop() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v&& T_ErrPolicy::is_noexcept) {
    if constexpr (T_ErrPolicy::enabled) {
      if (empty()) {
        return T_ErrPolicy::fail();
      }
    }
    m_top--;
    T temp = std::move(*m_top);
    std::destroy_at(m_top);

    return T_ErrPolicy::success(std::move(temp));
  }

  constexpr ErrPolicy_optional<T>::return_type
  try_pop() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    return pop<ErrPolicy_optional<T>>();
  }

  constexpr T
  pop_unchecked() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    static constexpr std::string_view funcInfo{PF_FUNC_INFO};
    return pop<ErrPolicy_nothing<T, funcInfo>>();
  }

  template <class T_ErrPolicy, class... V_args>
    requires ErrPolicy_c<T_ErrPolicy, pointer> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
  constexpr T_ErrPolicy::return_type
  emplace(V_args&&... args) PF_NOEXCEPT_COND(
      Traits::template is_nothrow_construct_v<V_args...>&& T_ErrPolicy::is_noexcept) {
    if constexpr (T_ErrPolicy::enabled) {
      if (full()) {
        return T_ErrPolicy::fail();
      }
    }
    new (m_top) T(std::forward<V_args>(args)...);
    pointer retVal = m_top;
    m_top++;

    return T_ErrPolicy::success(retVal);
  }

  template <class... V_args>
  constexpr ErrPolicy_throws<pointer, FullError>::return_type
  emplace(V_args&&... args) {
    return emplace<ErrPolicy_throws<pointer, FullError>>(std::forward<V_args>(args)...);
  }

  template <class... V_args>
  constexpr ErrPolicy_optional<pointer>::return_type
  try_emplace(V_args&&... args)
      PF_NOEXCEPT_COND(Traits::template is_nothrow_construct_v<V_args...>) {
    return emplace<ErrPolicy_optional<pointer>>(std::forward<V_args>(args)...);
  }

  template <class... V_args>
  pointer
  emplace_unchecked(V_args&&... args)
      PF_NOEXCEPT_COND(Traits::template is_nothrow_construct_v<V_args...>) {
    static constexpr std::string_view funcInfo{PF_FUNC_INFO};
    return emplace<ErrPolicy_nothing<pointer, funcInfo>>(std::forward<V_args>(args)...);
  }

  template <typename T_Range>
    requires CompatibleInputRange_c<Stack<T>, T_Range>
  constexpr void
  push_range_unchecked(T_Range&& range) {
    static constexpr std::string_view funcInfo{PF_FUNC_INFO};
    push_range<T_Range, ErrPolicy_nothing<void, funcInfo>>(std::forward<T_Range>(range));
  }

  template <typename T_Range, class T_ErrPolicy = ErrPolicy_throws<void, FullError>>
    requires CompatibleInputRange_c<Stack<T>, T_Range> && VoidErrPolicy_c<T_ErrPolicy> &&
             requires {
               { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
             }
  constexpr T_ErrPolicy::return_type
  push_range(T_Range&& range) {
    if constexpr (T_ErrPolicy::enabled) {
      if (std::ranges::size(range) > remaining()) {
        return T_ErrPolicy::fail();
      }
    }

    auto first = std::make_move_iterator(std::ranges::begin(range));
    auto last = std::make_move_iterator(std::ranges::end(range));
    for (; first != last; ++first) {
      emplace_unchecked(*first);
    }

    return T_ErrPolicy::success();
  }

  template <typename T_Range>
    requires CompatibleInputRange_c<Stack<T>, T_Range>
  [[nodiscard]] constexpr bool
  try_push_range(T_Range&& range) {
    return push_range<T_Range, ErrPolicy_optional<void>>(std::forward<T_Range>(range));
  }

  constexpr void
  clear() PF_NOEXCEPT {
    while (!empty()) {
      std::destroy_at(--m_top);
    }
  }

private:
  T* m_data{nullptr};
  T* m_top{nullptr};
  T* m_end{nullptr};

  [[nodiscard]] constexpr bool
  valid_init_() const PF_NOEXCEPT {
    return m_data != nullptr && m_end > m_data &&
           (reinterpret_cast<std::uintptr_t>(m_data) % alignof(T)) == 0;
  }
};

}
