module;

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include <PixelForge/containers/macros.hpp>
#include <PixelForge/core/macros.hpp>

export module PixelForge.containers:spscRingQueue;

import :utils.traits;

import PixelForge.core;

#define ASSUMPTIONS              \
  [[assume(m_data != nullptr)]]; \
  [[assume(m_mask > 0)]];

export namespace pf::adapters {

template <typename T>
class SPSCRingQueue {
public:
  struct Error : public Exception {
    Error() : Exception("SPSC ring queue error") {}
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

  constexpr SPSCRingQueue() PF_NOEXCEPT = default;

  /**
   *@brief ructs a queue over a typed ObjectStorage
   *
   * The storage's data pointer must be aligned for @p T and its size must be
   * a power of two (when @p T_isPowerOfTwo is true). Storage must outlive
   * the queue.
   */
  explicit constexpr SPSCRingQueue(ObjectStorage<storage_type> storage) PF_NOEXCEPT
    : m_data(pointer_cast<pointer>(storage.data)),
      m_mask(storage.size - 1) {
    PF_REQUIRE(valid_init());
  }

  constexpr ~SPSCRingQueue() PF_NOEXCEPT { clear(); }

  SPSCRingQueue(const SPSCRingQueue&) = delete;
  SPSCRingQueue(SPSCRingQueue&&) = delete;
  SPSCRingQueue&
  operator=(const SPSCRingQueue&) = delete;
  SPSCRingQueue&
  operator=(SPSCRingQueue&&) = delete;

  [[nodiscard]] constexpr pointer
  data() PF_NOEXCEPT {
    return m_data;
  }

  [[nodiscard]] constexpr pointer
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
    return m_back.load(std::memory_order_acquire) -
           m_front.load(std::memory_order_acquire);
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
    return m_data[idx(m_front.load(std::memory_order_acquire))];
  }

  [[nodiscard]] constexpr reference
  front() const PF_NOEXCEPT {
    ASSUMPTIONS;
    return m_data[idx(m_front.load(std::memory_order_acquire))];
  }

  [[nodiscard]] constexpr reference
  back() PF_NOEXCEPT {
    ASSUMPTIONS;
    PF_REQUIRE(!empty(), "queue empty");
    return m_data[idx(m_back.load(std::memory_order_acquire) - 1)];
  }

  [[nodiscard]] constexpr reference
  back() const PF_NOEXCEPT {
    ASSUMPTIONS;
    PF_REQUIRE(!empty(), "queue empty");
    return m_data[idx(m_back.load(std::memory_order_acquire) - 1)];
  }

  template <class ErrPolicy = ErrPolicyThrows<void, FullError>>
    requires VoidErrPolicy_c<ErrPolicy> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
    }
  constexpr ErrPolicy::return_type
  push(const T& value)
      PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v&& ErrPolicy::is_noexcept) {
    PF_CHECK_ERR_POLICY(ErrPolicy, full());

    static constexpr std::string_view func_info{PF_FUNC_INFO};
    static_cast<void>(emplace<ErrPolicyNothing<pointer, func_info>>(value));
    return ErrPolicy::success();
  }

  constexpr void
  push_unchecked(const T& value) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    push<ErrPolicyNothing<void, func_info>>(value);
  }

  constexpr ErrPolicyOptional<void>::return_type
  try_push(const T& val) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    return push<ErrPolicyOptional<void>>(val);
  }

  template <class ErrPolicy = ErrPolicyThrows<void, FullError>>
    requires VoidErrPolicy_c<ErrPolicy> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
    }
  constexpr ErrPolicy::return_type
  push(T&& val)
      PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v&& ErrPolicy::is_noexcept) {
    PF_CHECK_ERR_POLICY(ErrPolicy, full());

    static constexpr std::string_view func_info{PF_FUNC_INFO};
    static_cast<void>(
        emplace<ErrPolicyNothing<pointer, func_info>>(std::forward<T>(val)));
    return ErrPolicy::success();
  }

  constexpr void
  push_unchecked(T&& val) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    push<ErrPolicyNothing<void, func_info>>(std::forward<T>(val));
  }

  constexpr ErrPolicyOptional<void>::return_type
  try_push(T&& val) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    return push<ErrPolicyOptional<void>>(std::forward<T>(val));
  }

  template <class ErrPolicy, class... VArgs>
    requires ErrPolicy_c<ErrPolicy, pointer> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
    }
  constexpr ErrPolicy::return_type
  emplace(VArgs&&... args) PF_NOEXCEPT_COND(
      Traits::template is_nothrow_construct_v<V_args...>&& ErrPolicy::is_noexcept) {
    PF_CHECK_ERR_POLICY(ErrPolicy, full());

    ASSUMPTIONS;
    size_type head = m_back.load(std::memory_order_relaxed);
    std::construct_at(&m_data[idx(head)], std::forward<VArgs>(args)...);
    m_back.store(head + 1, std::memory_order_release);

    return ErrPolicy::success(&m_data[idx(head)]);
  }

  template <class... VArgs>
  pointer
  emplace_unchecked(VArgs&&... args)
      PF_NOEXCEPT_COND(Traits::template is_nothrow_construct_v<V_args...>) {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    return emplace<ErrPolicyNothing<pointer, func_info>, VArgs...>(
        std::forward<VArgs>(args)...);
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

  template <class ErrPolicy = ErrPolicyThrows<T, EmptyError>>
    requires ErrPolicy_c<ErrPolicy, T> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
    }
  constexpr ErrPolicy::return_type
  pop() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v&& ErrPolicy::is_noexcept) {
    PF_CHECK_ERR_POLICY(ErrPolicy, empty());

    ASSUMPTIONS;
    size_type tail = m_front.load(std::memory_order_relaxed);
    T temp = std::move(m_data[idx(tail)]);
    std::destroy_at(&m_data[idx(tail)]);
    m_front.store(tail + 1, std::memory_order_release);

    return ErrPolicy::success(std::move(temp));
  }

  constexpr T
  pop_unchecked() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    return pop<ErrPolicyNothing<T, func_info>>();
  }

  constexpr ErrPolicyOptional<T>::return_type
  try_pop() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    return pop<ErrPolicyOptional<T>>();
  }

  constexpr void
  clear() PF_NOEXCEPT {
    while (!empty()) {
      ASSUMPTIONS;
      std::destroy_at(&front());
      m_front.store(m_front.load(std::memory_order_relaxed) + 1,
                    std::memory_order_release);
    }
  }

private:
  pointer m_data{nullptr};
  size_type m_mask{0};

  PF_CACHE_LINE_ALIGN_VAR std::atomic<size_type> m_back{0};
  PF_CACHE_LINE_ALIGN_VAR std::atomic<size_type> m_front{0};

  [[nodiscard]] constexpr size_type
  idx(size_type num) const PF_NOEXCEPT {
    ASSUMPTIONS;
    return num & m_mask;
  }

  [[nodiscard]] constexpr bool
  valid_init() const PF_NOEXCEPT {
    return m_data != nullptr && capacity() > 0 && std::has_single_bit(capacity());
  }
};

}
