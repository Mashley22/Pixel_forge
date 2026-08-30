module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

#include <PixelForge/adapters/macros.hpp>
#include <PixelForge/core/macros.hpp>

export module PixelForge.adapters:ringQueue;

import :utils.traits;

import PixelForge.core;

#define ASSUMPTIONS              \
  [[assume(m_data != nullptr)]]; \
  [[assume(m_capMask > 0)]];

export namespace pf::adapters {

template <typename T, bool T_capacityPowOf2Value = false>
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

    static constexpr bool is_nothrow_copy_construct_v =
        std::is_nothrow_copy_constructible_v<T>;
    static constexpr bool is_nothrow_move_construct_v =
        std::is_nothrow_move_constructible_v<T>;
    template <typename... V_args>
    static constexpr bool is_nothrow_construct_v =
        std::is_nothrow_constructible_v<T, V_args...>;
  };

  PF_ADAPTERS_INHERIT_TRAITS(Traits);

  /**
   *@brief Constructs a queue over raw storage
   *
   * Elements are constructed in-place over the storage, so it must not hold
   * any live objects; typed buffers are therefore rejected (see the deleted
   * overload below). Storage must be aligned for @p T and outlive the queue
   */
  explicit constexpr RingQueue(void* pBuf,
                               size_type capacity,
                               size_type startIdx = 0) PF_NOEXCEPT
    : m_data(static_cast<pointer>(pBuf)),
      m_capMask(capacity - 1),
      m_front(startIdx),
      m_back(startIdx) {
    PF_REQUIRE(valid_init_());
  }

  /**@overload */
  explicit constexpr RingQueue(char* pBuf,
                               size_type capacity,
                               size_type startIdx = 0) PF_NOEXCEPT
    : RingQueue(static_cast<void*>(pBuf), capacity, startIdx) {}

  /**@overload */
  explicit constexpr RingQueue(std::byte* pBuf,
                               size_type capacity,
                               size_type startIdx = 0) PF_NOEXCEPT
    : RingQueue(static_cast<void*>(pBuf), capacity, startIdx) {}

  /**
   *@brief Constructs a queue over a byte span
   *
   * The element capacity is derived as @p buf.size() / sizeof(T); the span
   * must cover a whole number of elements and be aligned for @p T
   */
  explicit constexpr RingQueue(std::span<char> buf, size_type startIdx = 0) PF_NOEXCEPT
    : m_data(reinterpret_cast<pointer>(buf.data())),
      m_capMask((buf.size() / sizeof(T)) - 1),
      m_front(startIdx),
      m_back(startIdx) {
    PF_REQUIRE((buf.size() % sizeof(T)) == 0);
    PF_REQUIRE(valid_init_());
  }

  /**@overload */
  explicit constexpr RingQueue(std::span<std::byte> buf,
                               size_type startIdx = 0) PF_NOEXCEPT
    : m_data(reinterpret_cast<pointer>(buf.data())),
      m_capMask((buf.size() / sizeof(T)) - 1),
      m_front(startIdx),
      m_back(startIdx) {
    PF_REQUIRE((buf.size() % sizeof(T)) == 0);
    PF_REQUIRE(valid_init_());
  }

  constexpr ~RingQueue() PF_NOEXCEPT { clear(); }

  RingQueue(const RingQueue& other) =
      delete; // if interested in moving or copying the underlying contents
  // see \ref copy_contents_to or \ref move_contents_to

  RingQueue&
  operator=(const RingQueue& other) = delete;

  constexpr RingQueue(RingQueue&& other) PF_NOEXCEPT : m_data(other.m_data),
                                                       m_capMask(other.m_capMask),
                                                       m_front(other.m_front),
                                                       m_back(other.m_back) {
    other.m_data = nullptr;
    other.m_front = other.m_back = other.m_capMask = 0;
  }

  constexpr RingQueue&
  operator=(RingQueue&& other) PF_NOEXCEPT {
    PF_REQUIRE(this != &other);
    std::swap(m_data, other.m_data);
    std::swap(m_capMask, other.m_capMask);
    std::swap(m_front, other.m_front);
    std::swap(m_back, other.m_back);

    other.clear();

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

  [[nodiscard]] constexpr size_type
  capacity() const PF_NOEXCEPT {
    return m_capMask + 1;
  }

  [[nodiscard]] constexpr size_type
  size() const PF_NOEXCEPT {
    return m_back - m_front;
  }

  [[nodiscard]] constexpr size_type
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

  [[nodiscard]] constexpr reference
  front() PF_NOEXCEPT {
    ASSUMPTIONS;
    return m_data[toIdx_(m_front)];
  }

  [[nodiscard]] constexpr const_reference
  front() const PF_NOEXCEPT {
    ASSUMPTIONS;
    return m_data[toIdx_(m_front)];
  }

  [[nodiscard]] constexpr reference
  back() PF_NOEXCEPT {
    ASSUMPTIONS;
    PF_REQUIRE_ASSUME(m_back != 0);
    return m_data[toIdx_(m_back - 1)];
  }

  [[nodiscard]] constexpr const_reference
  back() const PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(m_back != 0);
    ASSUMPTIONS;
    return m_data[toIdx_(m_back - 1)];
  }

  /**
   *@brief by default throws \ref Error::Full if full, see \ref
   * emplace_unchecked
   */
  template <class... V_args>
  constexpr pointer
  emplace(V_args&&... args) {
    return emplace<ErrPolicy_throws<pointer, FullError>>(std::forward<V_args>(args)...);
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
    const size_type idx = toIdx_(m_back);
    new (&m_data[idx]) T(std::forward<V_args>(args)...);
    m_back++;

    return T_ErrPolicy::success(&m_data[idx]);
  }

  template <class... V_args>
  pointer
  emplace_unchecked(V_args&&... args)
      PF_NOEXCEPT_COND(Traits::template is_nothrow_construct_v<T>) {
    static constexpr std::string_view funcInfo{PF_FUNC_INFO};
    return emplace<ErrPolicy_nothing<pointer, funcInfo>, V_args...>(
        std::forward<V_args>(args)...);
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
    static constexpr std::string_view funcInfo{PF_FUNC_INFO};
    push<ErrPolicy_nothing<void, funcInfo>>(value);
  }

  /**
   *@overload
   *
   */
  constexpr void
  push_unchecked(T&& value) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    static constexpr std::string_view funcInfo{PF_FUNC_INFO};
    push<ErrPolicy_nothing<void, funcInfo>>(std::forward<T>(value));
  }

  /**
   *@brief returns false if full, see \ref emplace_unchecked
   */
  template <class... V_args>
  [[nodiscard]] constexpr std::optional<pointer>
  try_emplace(V_args&&... args)
      PF_NOEXCEPT_COND(Traits::template is_nothrow_construct_v<V_args...>) {
    return emplace<ErrPolicy_optional<pointer>, V_args...>(std::forward<V_args>(args)...);
  }

  /**
   *@brief returns false if full, see \ref push_unchecked
   *
   *@anchor try_push
   */
  [[nodiscard]] constexpr bool
  try_push(const T& val) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    return push<ErrPolicy_optional<void>>(val);
  }

  /**
   *@overload
   */
  [[nodiscard]] constexpr bool
  try_push(T&& val) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    return push<ErrPolicy_optional<void>>(std::forward<T>(val));
  }

  /**
   *@brief by default throws \ref FullError if full, see \ref push_unchecked
   *
   *@anchor push
   */
  template <class T_ErrPolicy = ErrPolicy_throws<void, FullError>>
    requires VoidErrPolicy_c<T_ErrPolicy> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
  constexpr T_ErrPolicy::return_type
  push(const T& value)
      PF_NOEXCEPT_COND(Traits::is_nothrow_copy_v&& T_ErrPolicy::is_noexcept) {
    if (full()) {
      return T_ErrPolicy::fail();
    }

    ASSUMPTIONS;
    emplace_unchecked(value);

    return T_ErrPolicy::success();
  }

  /**
   *@overload
   */
  template <class T_ErrPolicy = ErrPolicy_throws<void, FullError>>
    requires VoidErrPolicy_c<T_ErrPolicy> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
  constexpr T_ErrPolicy::return_type
  push(T&& val) PF_NOEXCEPT_COND(Traits::is_nothrow_move_v&& T_ErrPolicy::is_noexcept) {
    if (full()) {
      return T_ErrPolicy::fail();
    }

    ASSUMPTIONS;
    emplace_unchecked(std::forward<T>(val));

    return T_ErrPolicy::success();
  }

  /**
   *@brief forcefully adds to the back, if full it replaces the first element
   */
  template <class... V_args>
  constexpr reference
  force_emplace(V_args&&... args)
      PF_NOEXCEPT_COND(Traits::is_nothrow_construct_v<V_args...>) {
    if (full()) {
      ASSUMPTIONS;
      std::destroy_at(std::addressof(front()));
      m_front++;
    }
    return *emplace_unchecked(std::forward<V_args>(args)...);
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
    static constexpr std::string_view funcInfo{PF_FUNC_INFO};
    return pop<ErrPolicy_nothing<T, funcInfo>>();
  }

  /**
   *@brief pops from the front
   *
   *@returns false if failed
   *
   */
  [[nodiscard]] constexpr std::optional<T>
  try_pop() PF_NOEXCEPT {
    return pop<ErrPolicy_optional<T>>();
  }

  /**
   *@params val the element that was popped
   *
   */
  template <class T_ErrPolicy = ErrPolicy_throws<T, EmptyError>>
    requires ErrPolicy_c<T_ErrPolicy, T> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
  constexpr T_ErrPolicy::return_type
  pop() PF_NOEXCEPT_COND(T_ErrPolicy::is_noexcept&& Traits::is_nothrow_move_v) {
    if (empty()) {
      return T_ErrPolicy::fail();
    }

    ASSUMPTIONS;
    T temp = std::move(front());
    std::destroy_at(std::addressof(front()));
    m_front++;

    return T_ErrPolicy::success(std::move(temp));
  }

  template <typename T_Range>
    requires CompatibleInputRange_c<RingQueue<T>, T_Range>
  constexpr void
  push_range_unchecked(T_Range&& range) {
    static constexpr std::string_view funcInfo{PF_FUNC_INFO};
    push_range<T_Range, ErrPolicy_nothing<void, funcInfo>>(std::forward<T_Range>(range));
  }

  template <typename T_Range, class T_ErrPolicy = ErrPolicy_throws<void, FullError>>
    requires CompatibleInputRange_c<RingQueue<T>, T_Range> &&
             VoidErrPolicy_c<T_ErrPolicy> && requires {
               { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
             }
  constexpr T_ErrPolicy::return_type
  push_range(T_Range&& range) {
    if (std::ranges::size(range) > remaining()) {
      return T_ErrPolicy::fail();
    }
    auto first = std::make_move_iterator(std::ranges::begin(range));
    auto last = std::make_move_iterator(std::ranges::end(range));
    for (; first != last; ++first) {
      emplace_unchecked(*first);
    }
    return T_ErrPolicy::success();
  }

  template <typename T_Range>
    requires CompatibleInputRange_c<RingQueue<T>, T_Range>
  [[nodiscard]] constexpr bool
  try_push_range(T_Range&& range) {
    return push_range<T_Range, ErrPolicy_optional<void>>(std::forward<T_Range>(range));
  }

  constexpr void
  clear() PF_NOEXCEPT {
    size_type num_to_destroy = size();
    for (size_type i = 0; i < num_to_destroy; i++) {
      ASSUMPTIONS;
      std::destroy_at(&front());
      m_front++;
    }
    PF_REQUIRE(empty(), "implementation error!");
  }

private:
  pointer m_data;
  size_type m_capMask; // capacity - 1
  size_type m_front;   // index of front element (modulo capacity)
  size_type m_back;    // index one-past-back element (modulo capacity)
  // Both indices increment monotonically; toIdx_ applies modulo/wrap on access
  //
  [[nodiscard]] constexpr size_type
  toIdx_(size_type num) const PF_NOEXCEPT {
    ASSUMPTIONS;
    if constexpr (T_capacityPowOf2Value) {
      return num & m_capMask;
    } else {
      return num % capacity();
    }
  }

  [[nodiscard]] constexpr bool
  valid_init_() const PF_NOEXCEPT {
    return m_front == m_back && m_capMask > 0 && m_capMask != SIZE_MAX &&
           m_data != nullptr &&
           (reinterpret_cast<std::uintptr_t>(m_data) % alignof(T)) == 0 &&
           (!T_capacityPowOf2Value || ((capacity() & m_capMask) == 0));
  }
};

}
