module;

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#include <PixelForge/adapters/macros.hpp>
#include <PixelForge/core/macros.hpp>

export module PixelForge.adapters:spscQueue;

import :utils.traits;

import PixelForge.core;

#define ASSUMPTIONS              \
  [[assume(m_data != nullptr)]]; \
  [[assume(m_mask > 0)]];

export namespace pf::adapters {

template <typename T, bool T_isPowerOfTwo = false>
class SPSCQueue {
public:
  struct Error : public Exception {
    Error() : Exception("SPSC queue error") {}
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

  constexpr SPSCQueue() PF_NOEXCEPT = default;

  constexpr SPSCQueue(T* pBuf, size_type capacity) PF_NOEXCEPT : m_data(pBuf),
                                                                 m_mask(capacity - 1) {
    PF_REQUIRE(valid_init_());
  }

  constexpr SPSCQueue(std::span<T> buf) PF_NOEXCEPT : m_data(buf.data()),
                                                      m_mask(buf.size() - 1) {
    PF_REQUIRE(valid_init_());
  }

  constexpr ~SPSCQueue() PF_NOEXCEPT { clear(); }

  SPSCQueue(const SPSCQueue&) = delete;
  SPSCQueue(SPSCQueue&&) = delete;
  SPSCQueue&
  operator=(const SPSCQueue&) = delete;
  SPSCQueue&
  operator=(SPSCQueue&&) = delete;

  [[nodiscard]] constexpr pointer
  data() PF_NOEXCEPT {
    return m_data;
  }

  [[nodiscard]] constexpr const_pointer
  data() const PF_NOEXCEPT {
    return m_data;
  }

  [[nodiscard]] constexpr size_type
  capacity() const PF_NOEXCEPT {
    return m_mask + 1;
  }

  [[nodiscard]] size_type
  size() const PF_NOEXCEPT {
    // m_head/m_tail are monotonically increasing; unsigned subtraction stays
    // consistent across their wrap-around
    return m_head.load(std::memory_order_acquire) -
           m_tail.load(std::memory_order_acquire);
  }

  [[nodiscard]] size_type
  remaining() const PF_NOEXCEPT {
    return capacity() - size();
  }

  [[nodiscard]] bool
  empty() const PF_NOEXCEPT {
    return size() == 0;
  }

  [[nodiscard]] bool
  full() const PF_NOEXCEPT {
    return remaining() == 0;
  }

  [[nodiscard]] constexpr reference
  front() PF_NOEXCEPT {
    ASSUMPTIONS;
    return m_data[idx_(m_tail.load(std::memory_order_acquire))];
  }

  [[nodiscard]] constexpr const_reference
  front() const PF_NOEXCEPT {
    ASSUMPTIONS;
    return m_data[idx_(m_tail.load(std::memory_order_acquire))];
  }

  [[nodiscard]] constexpr reference
  back() PF_NOEXCEPT {
    ASSUMPTIONS;
    PF_REQUIRE(!empty(), "queue empty");
    return m_data[idx_(m_head.load(std::memory_order_acquire) - 1)];
  }

  [[nodiscard]] constexpr const_reference
  back() const PF_NOEXCEPT {
    ASSUMPTIONS;
    PF_REQUIRE(!empty(), "queue empty");
    return m_data[idx_(m_head.load(std::memory_order_acquire) - 1)];
  }

  template <class T_ErrPolicy = ErrPolicy_throws<void, FullError>>
    requires VoidErrPolicy_c<T_ErrPolicy> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
  constexpr T_ErrPolicy::return_type
  push(const T& value)
      PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v&& T_ErrPolicy::is_noexcept) {
    if (full()) {
      return T_ErrPolicy::fail();
    }

    static_cast<void>(emplace<ErrPolicy_nothing<pointer>>(value));
    return T_ErrPolicy::success();
  }

  constexpr void
  push_unchecked(const T& value) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    push<ErrPolicy_nothing<void>>(value);
  }

  constexpr ErrPolicy_optional<void>::return_type
  try_push(const T& val) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    return push<ErrPolicy_optional<void>>(val);
  }

  template <class T_ErrPolicy = ErrPolicy_throws<void, FullError>>
    requires VoidErrPolicy_c<T_ErrPolicy> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
  constexpr T_ErrPolicy::return_type
  push(T&& val)
      PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v&& T_ErrPolicy::is_noexcept) {
    if (full()) {
      return T_ErrPolicy::fail();
    }

    static_cast<void>(emplace<ErrPolicy_nothing<pointer>>(std::forward<T>(val)));
    return T_ErrPolicy::success();
  }

  constexpr void
  push_unchecked(T&& val) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    push<ErrPolicy_nothing<void>>(std::forward<T>(val));
  }

  constexpr ErrPolicy_optional<void>::return_type
  try_push(T&& val) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    return push<ErrPolicy_optional<void>>(std::forward<T>(val));
  }

  template <class T_ErrPolicy, class... V_args>
    requires ErrPolicy_c<T_ErrPolicy, pointer> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
  constexpr T_ErrPolicy::return_type
  emplace(V_args&&... args) PF_NOEXCEPT_COND(
      Traits::template is_nothrow_construct_v<V_args...>&& T_ErrPolicy::is_noexcept) {
    if (full()) {
      return T_ErrPolicy::fail();
    }

    ASSUMPTIONS;
    size_type head = m_head.load(std::memory_order_relaxed);
    new (&m_data[idx_(head)]) T(std::forward<V_args>(args)...);
    m_head.store(head + 1, std::memory_order_release);

    return T_ErrPolicy::success(&m_data[idx_(head)]);
  }

  template <class... V_args>
  pointer
  emplace_unchecked(V_args&&... args)
      PF_NOEXCEPT_COND(Traits::template is_nothrow_construct_v<V_args...>) {
    return emplace<ErrPolicy_nothing<pointer>, V_args...>(std::forward<V_args>(args)...);
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

  template <class T_ErrPolicy = ErrPolicy_throws<T, EmptyError>>
    requires ErrPolicy_c<T_ErrPolicy, T> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
  constexpr T_ErrPolicy::return_type
  pop() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v&& T_ErrPolicy::is_noexcept) {
    if (empty()) {
      return T_ErrPolicy::fail();
    }

    ASSUMPTIONS;
    size_type tail = m_tail.load(std::memory_order_relaxed);
    T temp = std::move(m_data[idx_(tail)]);
    std::destroy_at(&m_data[idx_(tail)]);
    m_tail.store(tail + 1, std::memory_order_release);

    return T_ErrPolicy::success(std::move(temp));
  }

  constexpr T
  pop_unchecked() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    return pop<ErrPolicy_nothing<T>>();
  }

  constexpr ErrPolicy_optional<T>::return_type
  try_pop() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    return pop<ErrPolicy_optional<T>>();
  }

  constexpr void
  clear() PF_NOEXCEPT {
    while (!empty()) {
      ASSUMPTIONS;
      std::destroy_at(&front());
      m_tail.store(m_tail.load(std::memory_order_relaxed) + 1, std::memory_order_release);
    }
  }

private:
  pointer m_data{nullptr};
  size_type m_mask{0};

  PF_CACHE_LINE_ALIGN std::atomic<size_type> m_head{0};
  PF_CACHE_LINE_ALIGN std::atomic<size_type> m_tail{0};

  [[nodiscard]] constexpr size_type
  idx_(size_type num) const PF_NOEXCEPT {
    ASSUMPTIONS;
    if constexpr (T_isPowerOfTwo) {
      return num & m_mask;
    } else {
      return num % capacity();
    }
  }

  [[nodiscard]] constexpr bool
  valid_init_() const PF_NOEXCEPT {
    return m_data != nullptr && capacity() > 0 &&
           (reinterpret_cast<std::uintptr_t>(m_data) % alignof(T)) == 0 &&
           (!T_isPowerOfTwo || ((capacity() & m_mask) == 0));
  }
};

}
