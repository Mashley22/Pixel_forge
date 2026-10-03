module;

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>

#include <PixelForge/containers/macros.hpp>
#include <PixelForge/core/macros.hpp>

export module PixelForge.containers:stack;

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
    using storage_type = T;

    static constexpr bool is_nothrow_copy_construct_v =
        std::is_nothrow_copy_constructible_v<T>;
    static constexpr bool is_nothrow_move_construct_v =
        std::is_nothrow_move_constructible_v<T>;
    template <typename... VArgs>
    static constexpr bool is_nothrow_construct_v =
        std::is_nothrow_constructible_v<T, VArgs...>;
  };

  PF_CONTAINERS_INHERIT_TRAITS(Traits);

  constexpr Stack() PF_NOEXCEPT = default;

  /**
   *@brief Constructs a stack over a typed ObjectStorage
   *
   * The storage's data pointer must be aligned for @p T. Storage must
   * outlive the stack.
   */
  explicit constexpr Stack(ObjectStorage<T> storage) PF_NOEXCEPT
    : m_data(pointer_cast<T*>(storage.data)),
      m_top(m_data),
      m_end(m_data + storage.size) {
    PF_REQUIRE(valid_init());
  }

  constexpr ~Stack() PF_NOEXCEPT { clear(); }

  Stack(const Stack&) = delete;

  constexpr Stack(Stack&& other) PF_NOEXCEPT : m_data(other.m_data),
                                               m_top(other.m_top),
                                               m_end(other.m_end) {
    other.m_data = nullptr;
    other.m_top = nullptr;
    other.m_end = nullptr;
  }

  constexpr Stack&
  operator=(const Stack& other) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    if (this == &other) return *this;
    PF_REQUIRE(other.size() <= capacity(), "not enough space");

    clear();

    for (pointer it = other.m_data; it != other.m_top; ++it) {
      std::construct_at(m_top, *it);
      m_top++;
    }

    return *this;
  }

  constexpr Stack&
  operator=(Stack&& other) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    if (this == &other) return *this;
    clear();
    m_data = other.m_data;
    m_top = other.m_top;
    m_end = other.m_end;
    other.m_data = nullptr;
    other.m_top = nullptr;
    other.m_end = nullptr;
    return *this;
  }

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

  template <class ErrPolicy = ErrPolicyThrows<void, FullError>>
    requires VoidErrPolicy_c<ErrPolicy> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
    }
  constexpr ErrPolicy::return_type
  push(const T& value)
      PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v&& ErrPolicy::is_noexcept) {
    PF_CHECK_ERR_POLICY(ErrPolicy, full());

    std::construct_at(m_top, value);
    m_top++;
    return ErrPolicy::success();
  }

  constexpr ErrPolicyOptional<void>::return_type
  try_push(const T& value) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    return push<ErrPolicyOptional<void>>(value);
  }

  constexpr void
  push_unchecked(const T& value) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    return push<ErrPolicyNothing<void, func_info>>(value);
  }

  template <class ErrPolicy = ErrPolicyThrows<void, FullError>>
    requires VoidErrPolicy_c<ErrPolicy> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
    }
  constexpr ErrPolicy::return_type
  push(T&& value)
      PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v&& ErrPolicy::is_noexcept) {
    PF_CHECK_ERR_POLICY(ErrPolicy, full());

    emplace_unchecked(std::forward<T>(value));
    return ErrPolicy::success();
  }

  constexpr void
  push_unchecked(T&& value) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    return push<ErrPolicyNothing<void, func_info>>(std::forward<T>(value));
  }

  constexpr ErrPolicyOptional<void>::return_type
  try_push(T&& value) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    return push<ErrPolicyOptional<void>>(std::forward<T>(value));
  }

  template <class ErrPolicy = ErrPolicyThrows<T, FullError>>
    requires ErrPolicy_c<ErrPolicy, T> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
    }
  constexpr ErrPolicy::return_type
  pop() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v&& ErrPolicy::is_noexcept) {
    PF_CHECK_ERR_POLICY(ErrPolicy, empty());

    m_top--;
    T temp = std::move(*m_top);
    std::destroy_at(m_top);

    return ErrPolicy::success(std::move(temp));
  }

  constexpr ErrPolicyOptional<T>::return_type
  try_pop() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    return pop<ErrPolicyOptional<T>>();
  }

  constexpr T
  pop_unchecked() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    return pop<ErrPolicyNothing<T, func_info>>();
  }

  template <class ErrPolicy, class... VArgs>
    requires ErrPolicy_c<ErrPolicy, pointer> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
    }
  constexpr ErrPolicy::return_type
  emplace(VArgs&&... args) PF_NOEXCEPT_COND(
      Traits::template is_nothrow_construct_v<V_args...>&& ErrPolicy::is_noexcept) {
    PF_CHECK_ERR_POLICY(ErrPolicy, full());

    std::construct_at(m_top, std::forward<VArgs>(args)...);
    pointer ret_val = m_top;
    m_top++;

    return ErrPolicy::success(ret_val);
  }

  template <class... VArgs>
  constexpr ErrPolicyThrows<pointer, FullError>::return_type
  emplace(VArgs&&... args) {
    return emplace<ErrPolicyThrows<pointer, FullError>>(std::forward<VArgs>(args)...);
  }

  template <class... VArgs>
  constexpr ErrPolicyOptional<pointer>::return_type
  try_emplace(VArgs&&... args)
      PF_NOEXCEPT_COND(Traits::template is_nothrow_construct_v<V_args...>) {
    return emplace<ErrPolicyOptional<pointer>>(std::forward<VArgs>(args)...);
  }

  template <class... VArgs>
  pointer
  emplace_unchecked(VArgs&&... args)
      PF_NOEXCEPT_COND(Traits::template is_nothrow_construct_v<V_args...>) {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    return emplace<ErrPolicyNothing<pointer, func_info>>(std::forward<VArgs>(args)...);
  }

  template <typename RangeT>
    requires CompatibleInputRange_c<Stack<T>, RangeT>
  constexpr void
  push_range_unchecked(RangeT&& range) {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    push_range<RangeT, ErrPolicyNothing<void, func_info>>(std::forward<RangeT>(range));
  }

  template <typename RangeT, class ErrPolicy = ErrPolicyThrows<void, FullError>>
    requires CompatibleInputRange_c<Stack<T>, RangeT> && VoidErrPolicy_c<ErrPolicy> &&
             requires {
               { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
             }
  constexpr ErrPolicy::return_type
  push_range(RangeT&& range) {
    PF_CHECK_ERR_POLICY(ErrPolicy, std::ranges::size(range) > remaining());

    auto first = std::make_move_iterator(std::ranges::begin(range));
    auto last = std::make_move_iterator(std::ranges::end(range));
    for (; first != last; ++first) {
      emplace_unchecked(*first);
    }

    return ErrPolicy::success();
  }

  template <typename RangeT>
    requires CompatibleInputRange_c<Stack<T>, RangeT>
  [[nodiscard]] constexpr bool
  try_push_range(RangeT&& range) {
    return push_range<RangeT, ErrPolicyOptional<void>>(std::forward<RangeT>(range));
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
  valid_init() const PF_NOEXCEPT {
    return m_data != nullptr && m_end > m_data &&
           (reinterpret_cast<std::uintptr_t>(m_data) % alignof(T)) == 0;
  }
};

}
