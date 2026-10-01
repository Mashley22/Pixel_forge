module;

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include <PixelForge/containers/macros.hpp>
#include <PixelForge/core/macros.hpp>

export module PixelForge.containers:llQueue;

import PixelForge.core;

export namespace pf::adapters {

/**
 *@brief a queue data class, represented via a linked list
 *  under the hood, intended for single threaded use
 *
 *@note A moved from, or default constructed object must be
 *  initialized by one of the move operations before use.
 *
 *@note The spare must be popped with \ref pop_spare before destruction.
 *
 */
template <typename T>
class LLQueue {
public:
  struct Node {
    Node* next{nullptr};
    T val;
  };

  struct Error : public Exception {};
  struct EmptyError : public Exception {
    static constexpr std::string_view what_arg = "LLQueue: Attempted pop while empty";
    EmptyError() : Exception(what_arg) {}
  };

  struct Traits {
    using ValueType = T;
    using SizeType = std::size_t;
    using DifferenceType = std::ptrdiff_t;
    using Reference = ValueType&;
    using ConstReference = const ValueType&;
    using Pointer = T*;
    using ConstPointer = const T*;
    using StorageType = Node;

    static constexpr bool is_nothrow_copy_construct_v =
        std::is_nothrow_copy_constructible_v<T>;
    static constexpr bool is_nothrow_move_construct_v =
        std::is_nothrow_move_constructible_v<T>;
    template <typename... VArgs>
    static constexpr bool is_nothrow_construct_v =
        std::is_nothrow_constructible_v<T, VArgs...>;
  };

  PF_CONTAINERS_INHERIT_TRAITS(Traits);

  LLQueue(const ObjectStorage<StorageType>& spare_storage)
    : m_front(NonNull<Node*>(pointer_cast<Node*>(spare_storage.data))),
      m_back(NonNull<Node*>(pointer_cast<Node*>(spare_storage.data))) {
    PF_REQUIRE(spare_storage.size == 1);
  }

  // A moved-from or otherwise null queue may only be destroyed or assigned a
  // non-null queue. Move assignment is required before any other operation.
  LLQueue() PF_NOEXCEPT = default;
  LLQueue(const LLQueue<T>&) = delete;
  LLQueue(LLQueue<T>&& other) PF_NOEXCEPT : m_front(other.m_front), m_back(other.m_back) {
    other.m_front = nullptr;
    other.m_back = nullptr;
    PF_REQUIRE(other.is_null());
  }

  LLQueue<T>&
  operator=(const LLQueue<T>&) = delete;
  LLQueue<T>&
  operator=(LLQueue<T>&& other) PF_NOEXCEPT {
    if (this != &other) {
      clear_();
      std::swap(m_front, other.m_front);
      std::swap(m_back, other.m_back);
    }
    return *this;
  }

  ~LLQueue() PF_NOEXCEPT { clear_(); }

  bool
  empty() PF_NOEXCEPT {
    return m_front->next == nullptr;
  }

  template <class... VArgs>
  void
  emplace(const ObjectStorage<StorageType>& storage, VArgs&&... args)
      PF_NOEXCEPT_COND(template Traits::is_nothrow_construct_v) {
    PF_REQUIRE_ASSUME(storage.size == 1);

    Node* new_node = std::construct_at(
        pointer_cast<Node*>(storage.data), nullptr, std::forward<VArgs>(args)...);

    m_back->next = new_node;
    m_back = NonNull<Node*>::from(new_node);
  }

  void
  push(const ObjectStorage<StorageType>& storage, T&& val)
      PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    emplace(storage, std::forward<T>(val));
  }

  void
  push(const ObjectStorage<StorageType>& storage, const T& val)
      PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    emplace(storage, val);
  }

  template <typename ErrPolicy = ErrPolicyThrows<NonNull<Node*>, EmptyError>>
    requires ErrPolicy_c<ErrPolicy, NonNull<Node*>> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::ReturnType>;
    }
  [[nodiscard]] typename ErrPolicy::ReturnType
  pop() PF_NOEXCEPT_COND(ErrPolicy::is_noexcept) {
    PF_REQUIRE_ASSUME(!is_null());
    NonNull<Node*> dummy_node{m_front};
    Node* next = dummy_node->next;
    PF_CHECK_ERR_POLICY(ErrPolicy, next == nullptr);

    dummy_node->val = std::move(next->val);
    m_front = NonNull<Node*>::from(next);

    return ErrPolicy::success(dummy_node);
  }

  [[nodiscard]] std::optional<NonNull<Node*>>
  try_pop() PF_NOEXCEPT {
    return pop<ErrPolicyOptional<NonNull<Node*>>>();
  }

  [[nodiscard]] NonNull<Node*>
  pop_unchecked() PF_NOEXCEPT {
    return pop<ErrPolicyNothing<NonNull<Node*>, EmptyError::what_arg>>();
  }

  [[nodiscard]] NonNull<Node*>
  pop_spare() PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(m_front != nullptr && empty());
    const auto ret_val = NonNull<Node*>(m_front);
    m_front = nullptr;
    m_back = nullptr;

    return ret_val;
  }

  [[nodiscard]] constexpr bool
  is_null() PF_NOEXCEPT {
    return m_front == nullptr || m_back == nullptr;
  }

private:
  void
  clear_() PF_NOEXCEPT {
    PF_REQUIRE(is_null(), "The spare should be popped before destruction");
  }

  Node* m_front{nullptr};
  Node* m_back{nullptr};
};

} // namespace pf::adapters
