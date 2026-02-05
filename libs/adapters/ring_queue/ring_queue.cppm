module;

#include <cstddef>
#include <type_traits>
#include <utility>
#include <optional>
#include <ranges>

#include <PixelForge/core/macros.hpp>
#include <PixelForge/adapters/macros.hpp>

export module PixelForge.adapters.ringQueue;

import PixelForge.adapters.utils.traits;

import PixelForge.core;

#define ASSUMPTIONS \
[[assume(m_data != nullptr)]]; \
[[assume(m_capMask > 0)]]; 

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
#define NOEXCEPT_MOVE 
#define NOEXCEPT_COPY
#define NOEXCEPT_CONSTRUCT(...)
#else
#define NOEXCEPT_MOVE noexcept(Traits::is_nothrow_move_v)
#define NOEXCEPT_COPY noexcept(Traits::is_nothrow_copy_v)
#define NOEXCEPT_CONSTRUCT(...) noexcept(Traits::template is_nothrow_construct_v<__VA_ARGS__>)
#endif

export namespace pf::adapters {

template<typename T, bool T_capacityPowOf2Value = false>
class RingQueue {
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
  
  constexpr
  RingQueue(T* pBuf, size_type capacity, size_type startIdx = 0) PF_NOEXCEPT
    : m_data(pBuf), m_capMask(capacity - 1), m_front(startIdx), m_back(startIdx) {
    PF_REQUIRE(valid_init_());
  }
  
  constexpr
  RingQueue(std::span<T> buf, size_type startIdx = 0) PF_NOEXCEPT
    : m_data(buf.data()), m_capMask(buf.size() - 1), m_front(startIdx), m_back(startIdx) {
    PF_REQUIRE(valid_init_());
  }

  constexpr 
  ~RingQueue(void) PF_NOEXCEPT {
    clear();
  }
  
  RingQueue(const RingQueue& other) = delete; // if interested in moving or copying the underlying contents
  // see \ref copy_contents_to or \ref move_contents_to

  RingQueue& operator=(const RingQueue& other) = delete;

  constexpr
  RingQueue(RingQueue&& other) PF_NOEXCEPT
    : m_data(other.data), m_capMask(other.m_capMask), m_front(other.m_front), m_back(other.m_back) {
    other.m_data = nullptr;
    other.m_front = other.m_back = other.m_capMask = 0;

    PF_REQUIRE(valid_init_() && !other.valid_init_(), "implementation error!");
  }

  constexpr RingQueue&
  operator=(RingQueue&& other) PF_NOEXCEPT
  {
    m_data = other.m_data;
    m_capMask = other.m_capMask;
    m_front = other.m_front;
    m_back = other.m_back;

    other.m_data = nullptr;
    other.m_front = other.m_back = other.m_capMask = 0;

    PF_REQUIRE(valid_init_() && !other.valid_init_(), "implementation error!");
  }

  [[nodiscard]] constexpr pointer
  data(void) PF_NOEXCEPT {
    return m_data;
  }

  [[nodiscard]] constexpr const_pointer
  data(void) const PF_NOEXCEPT {
    return m_data;
  }

  [[nodiscard]] constexpr size_type
  capacity(void) const PF_NOEXCEPT {
    return m_capMask + 1;
  }

  [[nodiscard]] constexpr size_type
  size(void) const PF_NOEXCEPT {
    return m_back - m_front;
  }

  [[nodiscard]] constexpr size_type
  remaining(void) const PF_NOEXCEPT {
    return capacity() - size();
  }

  [[nodiscard]] constexpr bool
  empty(void) const PF_NOEXCEPT {
    return size() == 0;
  }

  [[nodiscard]] constexpr bool
  full(void) const PF_NOEXCEPT {
    return size() >= capacity();
  }

  [[nodiscard]] constexpr reference
  front(void) PF_NOEXCEPT {
    return m_data[m_front & m_capMask];
  }

  [[nodiscard]] constexpr const_reference
  front(void) const PF_NOEXCEPT {
    return m_data[m_front & m_capMask];
  }
  
  [[nodiscard]] constexpr reference
  back(void) PF_NOEXCEPT {
    return m_data[(m_back - 1) & m_capMask]; 
  }

  [[nodiscard]] constexpr const_reference
  back(void) const PF_NOEXCEPT {
    return m_data[(m_back - 1) & m_capMask];
  }

  template<class... V_args>
  constexpr reference
  emplace_unchecked(V_args... args) NOEXCEPT_CONSTRUCT(V_args...) {
    PF_REQUIRE(!full());
    new(&m_data[m_back & m_capMask]) T(std::forward<V_args>(args)...);
    reference retVal = m_data[m_back & m_capMask];
    m_back++;
    return retVal;
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
  push_unchecked(const T& value) NOEXCEPT_COPY {
    PF_REQUIRE(!full());
    m_data[m_back & m_capMask] = value;
    m_back++;
  }

  /**
   *@overload
   *
  */
  constexpr void
  push_unchecked(T&& value) NOEXCEPT_MOVE {
    emplace(value);
  }

  /**
   *@brief returns false if full, see \ref emplace_unchecked
  */
  template<class... V_args>
  [[nodiscard]] constexpr std::optional<reference>
  try_emplace(V_args... args) NOEXCEPT_CONSTRUCT(V_args...) {
    if (full()) {
      return std::nullopt;
    }
    return emplace_unchecked(args...);
  }

  /**
   *@brief returns false if full, see \ref push_unchecked
   *
   *@anchor try_push
  */
  [[nodiscard]] constexpr bool
  try_push(const T& val) NOEXCEPT_MOVE {
    if (full()) {
      return false;
    }
    push_unchecked(val);
    return true;
  }
  
  /**
   *@overload
  */
  [[nodiscard]] constexpr bool
  try_push(T&& val) NOEXCEPT_MOVE {
    if (full()) {
      return false;
    }
    push_unchecked(val);
    return true;
  }

  /**
   *@brief throws \ref Error::Full if full, see \ref emplace_unchecked
  */
  template<class... V_args>
  constexpr reference
  emplace(V_args... args) {
    if (full()) {
      throw FullError{};
    }
    return emplace_unchecked(args...);
  }

  /**
   *@brief throws \ref FullError if full, see \ref push_unchecked
   *
   *@anchor push
  */
  constexpr void
  push(const T& val) {
    if (full()) {
      throw Error::Full;
    }
    push_unchecked(val);
  }
  
  /**
   *@overload
  */
  constexpr void
  push(T&& val) {
    if (full()) {
      throw Error::Full;
    }
    push_unchecked(val);
  }

  /**
   *@brief forcefully adds to the back, if full it replaces the first element  
  */
  template<class... V_args>
  constexpr reference
  force_emplace(V_args... args) NOEXCEPT_CONSTRUCT(V_args...) {
    if (full()) {
      m_front++;
    }
    return emplace_unchecked(args...);
  }

  /**
   *@brief forcefully adds to the back, if full it replaces the first element  
   *
   *@anchor force_push
  */
  constexpr void
  force_push(const T& val) NOEXCEPT_COPY {
    if (full()) { 
      m_front++;
    }
    push_unchecked(val);
  }
  
  /**
   *@overload
  */
  constexpr void
  force_push(T&& val) NOEXCEPT_MOVE {
    if (full()) {
      m_front++;
    }
    push_unchecked(val);
  }
  
  // The choice of this api is simply to always offer a noexcept way of popping the queue

  /**
   *@brief pops from the front
   *
  */
  constexpr T
  pop_unchecked(void) PF_NOEXCEPT {
    PF_REQUIRE(!empty());
    T temp = std::move(front());
    std::destroy_at(std::addressof(front())); // maybe not needed will see if compiler can optimise
    m_front++;
    return temp;
  }

  /**
   *@brief pops from the front
   *
   *@returns false if failed
   *
  */
  [[nodiscard]] constexpr std::optional<T>
  try_pop(void) PF_NOEXCEPT {
    if (empty()) {
      return std::nullopt;
    }
    
    return pop_unchecked();
  }

  /**
   *@params val the element that was popped
   *
  */
  constexpr T
  pop(T& val) {
    if (empty()) {
      throw EmptyError{};
    }
    
    return pop_unchecked(val);
  }
    
  template<typename T_Range>
  requires CompatibleInputRange_c<RingQueue<T>, T_Range>
  constexpr void
  push_range_unchecked(T_Range&& range) {
    for (const_reference val : range) {
      emplace_unchecked(val);
    }
  }
  
  template<typename T_Range>
  requires CompatibleInputRange_c<RingQueue<T>, T_Range>
  constexpr void
  push_range(T_Range&& range) {
    if (std::ranges::size(range) > remaining()) {
      throw FullError{};
    }
    push_range_unchecked(range);
  }

  template<typename T_Range>
  requires CompatibleInputRange_c<RingQueue<T>, T_Range>
  [[nodiscard]] constexpr bool
  try_push_range(T_Range&& range) {
    if (std::ranges::size(range) > remaining()) {
      return false;
    }
    push_range_unchecked(range);
  }
  
  constexpr void
  clear(void) PF_NOEXCEPT {
    size_type num_to_destroy = size();
    for (size_type i = 0; i < num_to_destroy; i++) { // abit safer than using the while(!empty())
      std::destroy_at(&front());
      m_front++;
    }
    PF_REQUIRE(empty(), "implementation error!");
  }

private:
  const_pointer m_data;
  size_type m_capMask; // capacity - 1
  size_type m_front; // is such that m_data[m_front] is the front() element
  size_type m_back; // is such that m_data[m_back] is the back() element
  // Both indices are incremented forever and only when accesses they are applied against the mask
  
  [[nodiscard]] constexpr bool
  valid_init_(void) const PF_NOEXCEPT {
    return m_front == m_back &&
           m_capMask > 0 &&
           m_data != nullptr &&
           m_front < m_capMask; // strictly speaking not 100% always required but is probably not good if violated
  }
};

}
