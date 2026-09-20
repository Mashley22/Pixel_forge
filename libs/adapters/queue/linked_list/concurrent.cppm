module;

#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>

#include <PixelForge/adapters/macros.hpp>
#include <PixelForge/core/macros.hpp>

export module PixelForge.adapters:concurrentLLQueue;

import PixelForge.core;

#define TRAITS                                          \
  struct Traits {                                       \
    using value_type = T;                               \
    using size_type = std::size_t;                      \
    using difference_type = std::ptrdiff_t;             \
    using reference = value_type&;                      \
    using const_reference = const value_type&;          \
    using pointer = T*;                                 \
    using const_pointer = const T*;                     \
    using storage_type = Node;                          \
                                                        \
    static constexpr bool is_nothrow_copy_construct_v = \
        std::is_nothrow_copy_constructible_v<T>;        \
    static constexpr bool is_nothrow_move_construct_v = \
        std::is_nothrow_move_constructible_v<T>;        \
    template <typename... V_args>                       \
    static constexpr bool is_nothrow_construct_v =      \
        std::is_nothrow_constructible_v<T, V_args...>;  \
  }

namespace pf {

namespace detail {

template <typename T>
class ConcurrentLLQueue {

public:
  struct Node {
    PF_CACHE_LINE_ALIGN_VAR
    std::atomic<Node*> next{nullptr};
    PF_CACHE_LINE_ALIGN_VAR
    T val;
  };

  TRAITS;

  PF_ADAPTERS_INHERIT_TRAITS(Traits);

  ConcurrentLLQueue(const ObjectStorage<storage_type>& dummyStorage)
    : m_front(NonNull<Node*>(pointer_cast<Node*>(dummyStorage.data))),
      m_back(NonNull<Node*>(pointer_cast<Node*>(dummyStorage.data))) {
    PF_REQUIRE(dummyStorage.size == 1);
  }

  ConcurrentLLQueue() PF_NOEXCEPT = default;
  ConcurrentLLQueue(const ConcurrentLLQueue<T>&) = delete;
  ConcurrentLLQueue(ConcurrentLLQueue<T>&& other) PF_NOEXCEPT : m_front(other.m_front),
                                                                m_back(other.m_back) {
    other.m_front = nullptr;
    other.m_back = nullptr;
    PF_REQUIRE(other.isNull_());
  }
  ConcurrentLLQueue<T>&
  operator=(const ConcurrentLLQueue<T>&) = delete;
  ConcurrentLLQueue<T>&
  operator=(ConcurrentLLQueue<T>&& other) PF_NOEXCEPT {
    if (this != &other) {
      clear_();
      std::swap(m_front, other.m_front);
      std::swap(m_back, other.m_front);
    }
    return *this;
  }

  ~ConcurrentLLQueue() PF_NOEXCEPT { clear_(); }

  bool
  empty() PF_NOEXCEPT {
    return m_front->next == nullptr;
  }

  template <class... V_Args>
  void
  sp_emplace(const ObjectStorage<storage_type>& storage, V_Args&&... args)
      PF_NOEXCEPT_COND(template Traits::is_nothrow_construct_v) {
    PF_REQUIRE_ASSUME(storage.size == 1);

    Node* newNode = std::construct_at(
        pointer_cast<Node*>(storage.data), nullptr, std::forward<V_Args>(args)...);

    m_back->next.store(newNode, std::memory_order_release);
    m_back = NonNull<Node*>::from(newNode);
  }

  void
  sp_push(const ObjectStorage<storage_type>& storage, T&& val)
      PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    sp_emplace(storage, std::forward<T>(val));
  }

  void
  sp_push(const ObjectStorage<storage_type>& storage, const T& val)
      PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    sp_emplace(storage, val);
  }

  template <typename T_ErrPolicy>
    requires ErrPolicy_c<T_ErrPolicy, NonNull<Node*>> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
  [[nodiscard]] typename T_ErrPolicy::return_type
  sp_pop() PF_NOEXCEPT_COND(T_ErrPolicy::is_noexcept) {
    NonNull<Node*> dummyNode{m_front};
    Node* next = dummyNode->next.load(std::memory_order_acquire);
    PF_CHECK_ERR_POLICY(T_ErrPolicy, next == nullptr);

    dummyNode->val = std::move(next->val);
    m_front = next;

    return T_ErrPolicy::success(dummyNode);
  }

private:
  [[nodiscard]] NonNull<Node*>
  pop_unchecked_() PF_NOEXCEPT {
    NonNull<Node*> dummyNode{m_front};
    Node* next = dummyNode->next.load(std::memory_order_acquire);

    dummyNode->val = std::move(next->val);
    m_front = next;

    return dummyNode;
  }

  void
  clear_() PF_NOEXCEPT {
    if (isNull_()) {
      return;
    }
    while (!empty()) {
      std::destroy_at<Node>(pop_unchecked_());
    }
    // The dummy has no constructed object
  }

  [[nodiscard]] constexpr bool
  isNull_() PF_NOEXCEPT {
    return m_front == nullptr || m_back == nullptr;
  }

  PF_CACHE_LINE_ALIGN_VAR
  Node* m_front{nullptr};

  PF_CACHE_LINE_ALIGN_VAR
  Node* m_back{nullptr};
};

}

export namespace adapters {

/**
 *@brief a queue data class, represented via a linked list
 *  under the hood, intended for muti threaded use with one 
 *  producer and one consumer
 *
 *@note A moved from, or default constructed object must be 
 *  initialized by one of the move operations before use.
 *
 */
template <typename T>
class SPSCLLQueue {
public:
  struct Error : public Exception {};
  struct EmptyError : public Exception {
    static constexpr std::string_view what_arg = "SPSCLLQueue: Attempted pop while empty";
    EmptyError() : Exception(what_arg) {}
  };

  using Node = detail::ConcurrentLLQueue<T>::Node;

  TRAITS;

  PF_ADAPTERS_INHERIT_TRAITS(Traits);

  SPSCLLQueue() PF_NOEXCEPT = default;
  SPSCLLQueue(const ObjectStorage<storage_type>& dummyStorage) PF_NOEXCEPT
    : m_base(dummyStorage) {}

  SPSCLLQueue(SPSCLLQueue<T>&) = delete;
  SPSCLLQueue&
  operator=(SPSCLLQueue<T>&) = delete;

  SPSCLLQueue(SPSCLLQueue<T>&&) PF_NOEXCEPT = default;
  SPSCLLQueue&
  operator=(SPSCLLQueue<T>&&) PF_NOEXCEPT = default;

  ~SPSCLLQueue() = default;

  bool
  empty() PF_NOEXCEPT {
    return m_base.empty();
  }

  template <class... V_Args>
  void
  emplace(const ObjectStorage<storage_type>& storage, V_Args&&... args)
      PF_NOEXCEPT_COND(template Traits::is_nothrow_construct_v) {
    return m_base.sp_emplace(storage, std::forward<V_Args>(args)...);
  }

  void
  push(const ObjectStorage<storage_type>& storage, T&& val)
      PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    return m_base.sp_push(storage, std::forward<T>(val));
  }

  void
  push(const ObjectStorage<storage_type>& storage, const T& val)
      PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    return m_base.sp_push(storage, val);
  }

  template <typename T_ErrPolicy = ErrPolicy_throws<NonNull<Node*>, EmptyError>>
    requires ErrPolicy_c<T_ErrPolicy, NonNull<Node*>> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
  [[nodiscard]] typename T_ErrPolicy::return_type
  pop() PF_NOEXCEPT_COND(T_ErrPolicy::is_noexcept) {
    return m_base.template sp_pop<T_ErrPolicy>();
  }

  [[nodiscard]] std::optional<NonNull<Node*>>
  try_pop() PF_NOEXCEPT {
    return pop<ErrPolicy_optional<NonNull<Node*>>>();
  }

  [[nodiscard]] NonNull<Node*>
  pop_unchecked() PF_NOEXCEPT {
    return pop<ErrPolicy_nothing<NonNull<Node*>, EmptyError::what_arg>>();
  }

private:
  using Base = detail::ConcurrentLLQueue<T>;
  Base m_base{};
};

}

}
