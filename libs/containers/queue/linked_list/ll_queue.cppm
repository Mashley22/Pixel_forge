module;

#include <concepts>
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
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = T*;
    using const_pointer = const T*;
    using storage_type = Node;

    static constexpr bool is_nothrow_copy_construct_v =
        std::is_nothrow_copy_constructible_v<T>;
    static constexpr bool is_nothrow_move_construct_v =
        std::is_nothrow_move_constructible_v<T>;
    template <typename... VArgs>
    static constexpr bool is_nothrow_construct_v =
        std::is_nothrow_constructible_v<T, VArgs...>;
  };

  PF_CONTAINERS_INHERIT_TRAITS(Traits);

  LLQueue(const ObjectStorage<storage_type>& spare_storage)
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
  emplace(const ObjectStorage<storage_type>& storage, VArgs&&... args)
      PF_NOEXCEPT_COND(template Traits::is_nothrow_construct_v) {
    PF_REQUIRE_ASSUME(storage.size == 1);

    Node* new_node = std::construct_at(
        pointer_cast<Node*>(storage.data), nullptr, std::forward<VArgs>(args)...);

    m_back->next = new_node;
    m_back = NonNull<Node*>::from(new_node);
  }

  void
  push(const ObjectStorage<storage_type>& storage, T&& val)
      PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    emplace(storage, std::forward<T>(val));
  }

  void
  push(const ObjectStorage<storage_type>& storage, const T& val)
      PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
    emplace(storage, val);
  }

  template <typename ErrPolicy = ErrPolicyThrows<NonNull<Node*>, EmptyError>>
    requires ErrPolicy_c<ErrPolicy, NonNull<Node*>> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
    }
  [[nodiscard]] typename ErrPolicy::return_type
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

export namespace pf::containers {

/**
 *@brief a queue represented by a linked list that owns its nodes, every node
 *  is allocated from, and returned to, a memory resource that the queue holds
 *  by value
 *
 *@note memory management is completely automatic: nodes go back to the
 *  resource on pop, on clear and on destruction, there is no spare to hand
 *  back as with @ref pf::adapters::LLQueue
 *
 *@note the resource is held by value, what it allocates from (e.g. a pmr
 *  pool) must outlive the queue
 *
 *@note swap and move semantics are not implemented yet: a queue is neither
 *  copyable, movable nor swappable, it only ever gives memory back on pop,
 *  on clear and on destruction
 *
 *@tparam T the queued value type
 *@tparam Resource a pf::mem::Resource_c, e.g.
 *         pf::mem::std_memory_resource_adapter<std::pmr::memory_resource>
 */
template <typename T, mem::Resource_c Resource>
class LLQueue {
public:
  struct Node {
    Node* next{nullptr};
    T val;
  };

  struct Error : public Exception {
    explicit Error(const std::string_view& msg) : Exception(msg) {}
  };
  struct EmptyError : public Error {
    static constexpr std::string_view what_arg = "LLQueue: Attempted pop while empty";
    EmptyError() : Error(what_arg) {}
  };
  struct AllocError : public Error {
    static constexpr std::string_view what_arg =
        "LLQueue: Memory resource failed to allocate a node";
    AllocError() : Error(what_arg) {}
  };

  struct Traits {
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = T*;
    using const_pointer = const T*;
    using storage_type = Node;
    using resource_type = Resource;

    static constexpr bool is_nothrow_copy_construct_v =
        std::is_nothrow_copy_constructible_v<T>;
    static constexpr bool is_nothrow_move_construct_v =
        std::is_nothrow_move_constructible_v<T>;
    /** whether allocate and deallocate of the resource are themselves noexcept */
    static constexpr bool is_nothrow_allocate_v =
        mem::is_noexcept_resource<Resource>::value;
    /** whether the resource can be moved into the queue without throwing */
    static constexpr bool is_nothrow_resource_move_v =
        std::is_nothrow_move_constructible_v<Resource>;
    template <typename... VArgs>
    static constexpr bool is_nothrow_construct_v =
        std::is_nothrow_constructible_v<T, VArgs...>;
  };

  PF_CONTAINERS_INHERIT_TRAITS(Traits);

  using resource_type = Traits::resource_type;

  /**
   *@brief constructs a queue holding a copy of @p resource, an rvalue is
   *  moved into the queue
   *
   *@note the ctor is only noexcept when there is no null check to make, see
   *      pf::mem::NullableResource_c
   */
  explicit LLQueue(Resource resource)
      PF_NOEXCEPT_COND(Traits::is_nothrow_resource_move_v &&
                       !mem::NullableResource_c<Resource>)
    : m_resource(std::move(resource)) {
    if constexpr (mem::NullableResource_c<Resource>) {
      PF_REQUIRE(!m_resource.is_null(), "LLQueue: null memory resource");
    }
  }

  LLQueue(const LLQueue&) = delete;
  LLQueue&
  operator=(const LLQueue&) = delete;

  LLQueue(LLQueue&&) = delete;
  LLQueue&
  operator=(LLQueue&&) = delete;

  ~LLQueue() PF_NOEXCEPT_COND(Traits::is_nothrow_allocate_v) { clear(); }

  [[nodiscard]] bool
  empty() const PF_NOEXCEPT {
    return m_front == nullptr;
  }

  [[nodiscard]] size_type
  size() const PF_NOEXCEPT {
    return m_size;
  }

  /**
   *@brief the resource nodes are allocated from and returned to
   */
  [[nodiscard]] Resource&
  resource() PF_NOEXCEPT {
    return m_resource;
  }

  [[nodiscard]] const Resource&
  resource() const PF_NOEXCEPT {
    return m_resource;
  }

  [[nodiscard]] reference
  front() PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(!empty());
    return m_front->val;
  }

  [[nodiscard]] const_reference
  front() const PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(!empty());
    return m_front->val;
  }

  [[nodiscard]] reference
  back() PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(!empty());
    return m_back->val;
  }

  [[nodiscard]] const_reference
  back() const PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(!empty());
    return m_back->val;
  }

  /**
   *@brief constructs a value in place at the back of the queue, the node it
   *  lives in is taken from the resource
   *
   *@tparam ErrPolicy failure policy for a failed allocation, defaults to
   *         throwing @ref AllocError
   */
  template <typename ErrPolicy = ErrPolicyThrows<void, AllocError>, typename... VArgs>
    requires VoidErrPolicy_c<ErrPolicy> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
    }
  ErrPolicy::return_type
  emplace(VArgs&&... args) PF_NOEXCEPT_COND(
      Traits::is_nothrow_allocate_v&& Traits::template is_nothrow_construct_v<VArgs...>&&
          ErrPolicy::is_noexcept) {
    PF_REQUIRE_ASSUME(valid_());

    Buffer buffer = m_resource.allocate(sizeof(Node), alignof(Node));
    PF_CHECK_ERR_POLICY(ErrPolicy, buffer.is_null());
    PF_REQUIRE_ASSUME(!buffer.is_null());

    link_(std::construct_at(
        pointer_cast<Node*>(buffer.data), nullptr, std::forward<VArgs>(args)...));

    return ErrPolicy::success();
  }

  /**
   *@brief constructs a value in place at the back of the queue, a failed
   *  allocation is reported by returning false
   */
  template <typename... VArgs>
  ErrPolicyOptional<void>::return_type
  try_emplace(VArgs&&... args) PF_NOEXCEPT_COND(
      Traits::is_nothrow_allocate_v&& Traits::template is_nothrow_construct_v<VArgs...>) {
    return emplace<ErrPolicyOptional<void>>(std::forward<VArgs>(args)...);
  }

  template <typename... VArgs>
  void
  emplace_unchecked(VArgs&&... args) PF_NOEXCEPT_COND(
      Traits::is_nothrow_allocate_v&& Traits::template is_nothrow_construct_v<VArgs...>) {
    emplace<ErrPolicyNothing<void, AllocError::what_arg>>(std::forward<VArgs>(args)...);
  }

  template <typename ErrPolicy = ErrPolicyThrows<void, AllocError>>
    requires VoidErrPolicy_c<ErrPolicy> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
    }
  ErrPolicy::return_type
  push(T&& val)
      PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v&&
                           Traits::is_nothrow_allocate_v&& ErrPolicy::is_noexcept) {
    return emplace<ErrPolicy>(std::forward<T>(val));
  }

  template <typename ErrPolicy = ErrPolicyThrows<void, AllocError>>
    requires VoidErrPolicy_c<ErrPolicy> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
    }
  ErrPolicy::return_type
  push(const T& val)
      PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v&&
                           Traits::is_nothrow_allocate_v&& ErrPolicy::is_noexcept) {
    return emplace<ErrPolicy>(val);
  }

  ErrPolicyOptional<void>::return_type
  try_push(T&& val) PF_NOEXCEPT_COND(
      Traits::is_nothrow_move_construct_v&& Traits::is_nothrow_allocate_v) {
    return push<ErrPolicyOptional<void>>(std::forward<T>(val));
  }

  ErrPolicyOptional<void>::return_type
  try_push(const T& val) PF_NOEXCEPT_COND(
      Traits::is_nothrow_copy_construct_v&& Traits::is_nothrow_allocate_v) {
    return push<ErrPolicyOptional<void>>(val);
  }

  void
  push_unchecked(T&& val) PF_NOEXCEPT_COND(
      Traits::is_nothrow_move_construct_v&& Traits::is_nothrow_allocate_v) {
    push<ErrPolicyNothing<void, AllocError::what_arg>>(std::forward<T>(val));
  }

  void
  push_unchecked(const T& val) PF_NOEXCEPT_COND(
      Traits::is_nothrow_copy_construct_v&& Traits::is_nothrow_allocate_v) {
    push<ErrPolicyNothing<void, AllocError::what_arg>>(val);
  }

  /**
   *@brief removes the value at the front of the queue, its node is destroyed
   *  and returned to the resource
   *
   *@tparam ErrPolicy failure policy for an empty queue, defaults to throwing
   *         @ref EmptyError
   */
  template <typename ErrPolicy = ErrPolicyThrows<T, EmptyError>>
    requires ErrPolicy_c<ErrPolicy, T> && requires {
      { ErrPolicy::fail() } -> std::same_as<typename ErrPolicy::return_type>;
    }
  [[nodiscard]] ErrPolicy::return_type
  pop() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v&&
                             Traits::is_nothrow_allocate_v&& ErrPolicy::is_noexcept) {
    PF_CHECK_ERR_POLICY(ErrPolicy, empty());

    return ErrPolicy::success(pop_());
  }

  [[nodiscard]] ErrPolicyOptional<T>::return_type
  try_pop() PF_NOEXCEPT_COND(
      Traits::is_nothrow_move_construct_v&& Traits::is_nothrow_allocate_v) {
    return pop<ErrPolicyOptional<T>>();
  }

  [[nodiscard]] T
  pop_unchecked() PF_NOEXCEPT_COND(
      Traits::is_nothrow_move_construct_v&& Traits::is_nothrow_allocate_v) {
    return pop<ErrPolicyNothing<T, EmptyError::what_arg>>();
  }

  /**
   *@brief destroys every value in the queue and returns all nodes to the
   *  resource
   */
  void
  clear() PF_NOEXCEPT_COND(Traits::is_nothrow_allocate_v) {
    while (!empty()) {
      static_cast<void>(pop_());
    }
  }

private:
  /**
   *@brief takes the front value, then destroys the node and returns it to the
   *  resource
   */
  [[nodiscard]] T
  pop_() PF_NOEXCEPT_COND(
      Traits::is_nothrow_move_construct_v&& Traits::is_nothrow_allocate_v) {
    PF_REQUIRE_ASSUME(!empty() && valid_());

    Node* const node = m_front;
    T val = std::move(node->val);

    m_front = node->next;
    if (m_front == nullptr) {
      m_back = nullptr;
    }
    m_size--;

    std::destroy_at(node);
    m_resource.deallocate(node_buffer_(node));

    return val;
  }

  void
  link_(Node* node) PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(node != nullptr);

    if (m_back == nullptr) {
      m_front = node;
    } else {
      m_back->next = node;
    }
    m_back = node;
    m_size++;
  }

  [[nodiscard]] static Buffer
  node_buffer_(Node* node) PF_NOEXCEPT {
    return Buffer::from(pointer_cast<std::byte*>(node), sizeof(Node));
  }

  /** a resource that reports itself null cannot be used, nor can the queue be */
  [[nodiscard]] bool
  valid_() const PF_NOEXCEPT {
    if constexpr (mem::NullableResource_c<Resource>) {
      return !m_resource.is_null();
    }
    return true;
  }

  Node* m_front{nullptr};
  Node* m_back{nullptr};
  size_type m_size{0};
  /**
   * owned by value, what it allocates from must outlive the queue
   *
   * @note no_unique_address lets an empty resource, a mem::ResourceRef for
   *  instance, share its address with another member rather than costing a
   *  byte of its own
   */
  [[no_unique_address]] Resource m_resource;
};

} // namespace pf::containers
