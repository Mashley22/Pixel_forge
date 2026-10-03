module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

#include <PixelForge/containers/macros.hpp>
#include <PixelForge/core/macros.hpp>

export module PixelForge.containers:ringQueue;

import :utils.traits;

import PixelForge.core;

#define ASSUMPTIONS              \
  [[assume(m_data != nullptr)]]; \
  [[assume(m_cap_mask > 0)]];

export namespace pf::adapters {

template <typename T>
class RingQueue {
public:
  struct Error : public Exception {
    Error() : Exception("Ring queue error") {}
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

  /**
   *@brief Constructs a queue over a typed ObjectStorage
   *
   * The storage's data Pointer must be aligned for @p T and its size must be
   * a power of two (when @p T_capacityPowOf2Value is true). Storage must
   * outlive the queue.
   */
  explicit constexpr RingQueue(ObjectStorage<StorageType> storage,
                               SizeType start_idx = 0) PF_NOEXCEPT
    : m_data(pointer_cast<Pointer>(storage.data)),
      m_cap_mask(storage.size - 1),
      m_front(start_idx),
      m_back(start_idx) {
    PF_REQUIRE(valid_init());
  }

  constexpr ~RingQueue() PF_NOEXCEPT { clear(); }

  RingQueue(const RingQueue&) = delete;

  constexpr RingQueue&
  operator=(const RingQueue& other)
      PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    PF_REQUIRE(this != &other);
    PF_REQUIRE(other.size() <= this->capacity());

    this->clear();

    for (SizeType i = other.m_front; i < other.m_back; i++) {
      this->emplace_unchecked(other.m_data[other.to_idx(i)]);
    }

    return *this;
  }

  constexpr RingQueue(RingQueue&& other) PF_NOEXCEPT : m_data(other.m_data),
                                                       m_cap_mask(other.m_cap_mask),
                                                       m_front(other.m_front),
                                                       m_back(other.m_back) {
    other.m_data = nullptr;
    other.m_front = other.m_back = other.m_cap_mask = 0;
  }

  constexpr RingQueue&
  operator=(RingQueue&& other) PF_NOEXCEPT {
    PF_REQUIRE(this != &other);
    std::swap(m_data, other.m_data);
    std::swap(m_cap_mask, other.m_cap_mask);
    std::swap(m_front, other.m_front);
    std::swap(m_back, other.m_back);

    other.clear();

    return *this;
  }

  [[nodiscard]] constexpr Pointer
  data() PF_NOEXCEPT {
    return m_data;
  }

  [[nodiscard]] constexpr ConstPointer
  data() const PF_NOEXCEPT {
    return m_data;
  }

  [[nodiscard]] constexpr SizeType
  capacity() const PF_NOEXCEPT {
    return m_cap_mask + 1;
  }

  [[nodiscard]] constexpr SizeType
  size() const PF_NOEXCEPT {
    return m_back - m_front;
  }

  [[nodiscard]] constexpr SizeType
  remaining() const PF_NOEXCEPT {
    return capacity() - size();
  }

  [[nodiscard]] constexpr bool
  empty() const PF_NOEXCEPT {
    return size() == 0;
  }

  [[nodiscard]] constexpr bool
  full() const PF_NOEXCEPT {
    return size() >= capacity();
  }

  [[nodiscard]] constexpr Reference
  front() PF_NOEXCEPT {
    ASSUMPTIONS;
    return m_data[to_idx(m_front)];
  }

  [[nodiscard]] constexpr ConstReference
  front() const PF_NOEXCEPT {
    ASSUMPTIONS;
    return m_data[to_idx(m_front)];
  }

  [[nodiscard]] constexpr Reference
  back() PF_NOEXCEPT {
    ASSUMPTIONS;
    PF_REQUIRE_ASSUME(m_back != 0);
    return m_data[to_idx(m_back - 1)];
  }

  [[nodiscard]] constexpr ConstReference
  back() const PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(m_back != 0);
    ASSUMPTIONS;
    return m_data[to_idx(m_back - 1)];
  }

  /**
   *@brief by default throws \ref Error::Full if full, see \ref
   * emplace_unchecked
   */
  template <class... VArgs>
  constexpr Pointer
  emplace(VArgs&&... args) {
    return emplace<ErrPolicyThrows<Pointer, FullError>>(std::forward<VArgs>(args)...);
  }

  template <class ErrPolicy, class... VArgs>
    requires ErrPolicy_c<ErrPolicy, Pointer> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::ReturnType>;
    }
  constexpr ErrPolicy::ReturnType
  emplace(VArgs&&... args) PF_NOEXCEPT_COND(
      Traits::template is_nothrow_construct_v<V_args...>&& ErrPolicy::is_noexcept) {
    PF_CHECK_ERR_POLICY(ErrPolicy, full());

    ASSUMPTIONS;
    const SizeType idx = to_idx(m_back);
    std::construct_at(&m_data[idx], std::forward<VArgs>(args)...);
    m_back++;

    return ErrPolicy::success(&m_data[idx]);
  }

  template <class... VArgs>
  Pointer
  emplace_unchecked(VArgs&&... args)
      PF_NOEXCEPT_COND(Traits::template is_nothrow_construct_v<T>) {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    return emplace<ErrPolicyNothing<Pointer, func_info>, VArgs...>(
        std::forward<VArgs>(args)...);
  }

  /**
   *@brief adds an element dumbly to the tail
   *
   *@anchor push_unchecked
   *
   *@warning undefined if \ref size() == \ref capacity()
   *
   */
  constexpr void
  push_unchecked(const T& value) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    push<ErrPolicyNothing<void, func_info>>(value);
  }

  /**
   *@overload
   *
   */
  constexpr void
  push_unchecked(T&& value) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    push<ErrPolicyNothing<void, func_info>>(std::forward<T>(value));
  }

  /**
   *@brief returns false if full, see \ref emplace_unchecked
   */
  template <class... VArgs>
  [[nodiscard]] constexpr std::optional<Pointer>
  try_emplace(VArgs&&... args)
      PF_NOEXCEPT_COND(Traits::template is_nothrow_construct_v<V_args...>) {
    return emplace<ErrPolicyOptional<Pointer>, VArgs...>(std::forward<VArgs>(args)...);
  }

  /**
   *@brief returns false if full, see \ref push_unchecked
   *
   *@anchor try_push
   */
  [[nodiscard]] constexpr bool
  try_push(const T& val) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    return push<ErrPolicyOptional<void>>(val);
  }

  /**
   *@overload
   */
  [[nodiscard]] constexpr bool
  try_push(T&& val) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    return push<ErrPolicyOptional<void>>(std::forward<T>(val));
  }

  /**
   *@brief by default throws \ref FullError if full, see \ref push_unchecked
   *
   *@anchor push
   */
  template <class ErrPolicy = ErrPolicyThrows<void, FullError>>
    requires VoidErrPolicy_c<ErrPolicy> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::ReturnType>;
    }
  constexpr ErrPolicy::ReturnType
  push(const T& value)
      PF_NOEXCEPT_COND(Traits::is_nothrow_copy_v&& ErrPolicy::is_noexcept) {

    PF_CHECK_ERR_POLICY(ErrPolicy, full());

    ASSUMPTIONS;
    emplace_unchecked(value);

    return ErrPolicy::success();
  }

  /**
   *@overload
   */
  template <class ErrPolicy = ErrPolicyThrows<void, FullError>>
    requires VoidErrPolicy_c<ErrPolicy> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::ReturnType>;
    }
  constexpr ErrPolicy::ReturnType
  push(T&& val) PF_NOEXCEPT_COND(Traits::is_nothrow_move_v&& ErrPolicy::is_noexcept) {
    PF_CHECK_ERR_POLICY(ErrPolicy, full());

    ASSUMPTIONS;
    emplace_unchecked(std::forward<T>(val));

    return ErrPolicy::success();
  }

  /**
   *@brief forcefully adds to the back, if full it replaces the first element
   */
  template <class... VArgs>
  constexpr Reference
  force_emplace(VArgs&&... args)
      PF_NOEXCEPT_COND(Traits::is_nothrow_construct_v<V_args...>) {
    if (full()) {
      ASSUMPTIONS;
      std::destroy_at(std::addressof(front()));
      m_front++;
    }
    return *emplace_unchecked(std::forward<VArgs>(args)...);
  }

  /**
   *@brief forcefully adds to the back, if full it replaces the first element
   *
   *@anchor force_push
   */
  constexpr void
  force_push(const T& val) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    if (full()) {
      ASSUMPTIONS;
      std::destroy_at(std::addressof(front()));
      m_front++;
    }
    push_unchecked(val);
  }

  /**
   *@overload
   */
  constexpr void
  force_push(T&& val) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    if (full()) {
      ASSUMPTIONS;
      std::destroy_at(std::addressof(front()));
      m_front++;
    }
    push_unchecked(std::forward<T>(val));
  }

  // The choice of this api is simply to always offer a noexcept way of popping
  // the queue

  /**
   *@brief pops from the front
   *
   */
  constexpr T
  pop_unchecked() PF_NOEXCEPT {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    return pop<ErrPolicyNothing<T, func_info>>();
  }

  /**
   *@brief pops from the front
   *
   *@returns false if failed
   *
   */
  [[nodiscard]] constexpr std::optional<T>
  try_pop() PF_NOEXCEPT {
    return pop<ErrPolicyOptional<T>>();
  }

  /**
   *@params val the element that was popped
   *
   */
  template <class ErrPolicy = ErrPolicyThrows<T, EmptyError>>
    requires ErrPolicy_c<ErrPolicy, T> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::ReturnType>;
    }
  constexpr ErrPolicy::ReturnType
  pop() PF_NOEXCEPT_COND(ErrPolicy::is_noexcept&& Traits::is_nothrow_move_v) {
    PF_CHECK_ERR_POLICY(ErrPolicy, empty());

    ASSUMPTIONS;
    T temp = std::move(front());
    std::destroy_at(std::addressof(front()));
    m_front++;

    return ErrPolicy::success(std::move(temp));
  }

  template <typename RangeT>
    requires CompatibleInputRange_c<RingQueue<T>, RangeT>
  constexpr void
  push_range_unchecked(RangeT&& range) {
    static constexpr std::string_view func_info{PF_FUNC_INFO};
    push_range<RangeT, ErrPolicyNothing<void, func_info>>(std::forward<RangeT>(range));
  }

  template <typename RangeT, class ErrPolicy = ErrPolicyThrows<void, FullError>>
    requires CompatibleInputRange_c<RingQueue<T>, RangeT> && VoidErrPolicy_c<ErrPolicy> &&
             requires {
               { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::ReturnType>;
             }
  constexpr ErrPolicy::ReturnType
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
    requires CompatibleInputRange_c<RingQueue<T>, RangeT>
  [[nodiscard]] constexpr bool
  try_push_range(RangeT&& range) {
    return push_range<RangeT, ErrPolicyOptional<void>>(std::forward<RangeT>(range));
  }

  constexpr void
  clear() PF_NOEXCEPT {
    SizeType num_to_destroy = size();
    for (SizeType i = 0; i < num_to_destroy; i++) {
      ASSUMPTIONS;
      std::destroy_at(&front());
      m_front++;
    }
    PF_REQUIRE(empty(), "implementation error!");
  }

private:
  Pointer m_data;
  SizeType m_cap_mask; // capacity - 1
  SizeType m_front;    // index of front element (modulo capacity)
  SizeType m_back;     // index one-past-back element (modulo capacity)
  // Both indices increment monotonically; toIdx_ applies modulo/wrap on access
  //
  [[nodiscard]] constexpr SizeType
  to_idx(SizeType num) const PF_NOEXCEPT {
    ASSUMPTIONS;
    return num & m_cap_mask;
  }

  [[nodiscard]] constexpr bool
  valid_init() const PF_NOEXCEPT {
    return m_front == m_back && m_cap_mask > 0 && m_cap_mask != SIZE_MAX &&
           m_data != nullptr && std::has_single_bit(capacity());
  }
};

}
