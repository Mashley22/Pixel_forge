module;

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include <PixelForge/containers/macros.hpp>
#include <PixelForge/core/macros.hpp>

export module PixelForge.containers:concurrentLLQueue;

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

  PF_CONTAINERS_INHERIT_TRAITS(Traits);

private:
  /**
   *@brief the head of the queue, the node it points to holds no value
   *  and is a spare
   */
  class SpFront {
  public:
    SpFront() PF_NOEXCEPT = default;
    SpFront(const ObjectStorage<storage_type>& spareStorage)
      : m_front(pointer_cast<Node*>(spareStorage.data)) {
      PF_REQUIRE(spareStorage.size == 1);
    }

    SpFront(const SpFront&) = delete;
    SpFront&
    operator=(const SpFront&) = delete;

    SpFront(SpFront&& other) PF_NOEXCEPT : m_front(other.m_front) {
      other.m_front = nullptr;
    }

    SpFront&
    operator=(SpFront&& other) PF_NOEXCEPT {
      if (this != &other) {
        clear_();
        std::swap(m_front, other.m_front);
      }
      return *this;
    }

    ~SpFront() PF_NOEXCEPT { clear_(); }

    [[nodiscard]] bool
    isNull() const PF_NOEXCEPT {
      return m_front == nullptr;
    }

    [[nodiscard]] bool
    empty() const PF_NOEXCEPT {
      return m_front->next.load(std::memory_order::acquire) == nullptr;
    }

    /**
     *@brief moves the value at the front into the head and hands the head
     *  over, the head it vacated is left holding no value
     *
     *@returns nullptr when the queue is empty
     */
    [[nodiscard]] Node*
    pop() PF_NOEXCEPT {
      PF_REQUIRE_ASSUME(!isNull());
      Node* const head = m_front;
      Node* const next = head->next.load(std::memory_order_acquire);
      if (next == nullptr) {
        return nullptr;
      }

      // the head holds no object, so the value is constructed into it, and the
      // object it is taken from is destroyed, leaving the new head with none
      std::construct_at(&head->val, std::move(next->val));
      std::destroy_at(&next->val);
      m_front = next;

      return head;
    }

    /**
     *@brief releases the head, it is a spare, the queue must be drained
     */
    [[nodiscard]] Node*
    popSpare() PF_NOEXCEPT {
      PF_REQUIRE_ASSUME(!isNull() && empty());
      Node* const head = m_front;
      m_front = nullptr;
      return head;
    }

  private:
    void
    clear_() PF_NOEXCEPT {
      PF_REQUIRE_ASSUME(
          isNull(),
          "linked list queue adapter must be cleaned up before destructor is called"
          "it should be used as part of a class that pops the spare before "
          "destruction");
    }

    Node* m_front{nullptr};
  };

  /**
   *@brief the tail of the queue, for use by a single producer
   */
  class SpBack {
  public:
    SpBack() PF_NOEXCEPT = default;
    SpBack(const ObjectStorage<storage_type>& spareStorage)
      : m_back(pointer_cast<Node*>(spareStorage.data)) {
      PF_REQUIRE(spareStorage.size == 1);
    }

    ~SpBack() PF_NOEXCEPT = default;

    SpBack(const SpBack&) = delete;
    SpBack&
    operator=(const SpBack&) = delete;

    SpBack(SpBack&& other) PF_NOEXCEPT : m_back(other.m_back) { other.m_back = nullptr; }

    SpBack&
    operator=(SpBack&& other) PF_NOEXCEPT {
      if (this != &other) {
        PF_REQUIRE_ASSUME(isNull_());
        std::swap(m_back, other.m_back);
      }
      return *this;
    }

    [[nodiscard]] Node*
    link(Node* newNode) PF_NOEXCEPT {
      PF_REQUIRE_ASSUME(!isNull_());
      m_back->next.store(newNode, std::memory_order::release);
      m_back = newNode;
      return newNode;
    }

    void
    makeNull() PF_NOEXCEPT {
      m_back = nullptr;
    }

  private:
    [[nodiscard]] bool
    isNull_() PF_NOEXCEPT {
      return m_back == nullptr;
    }

    Node* m_back{nullptr};
  };

  /**
   *@brief the tail of the queue, for use by multiple producers
   *
   *@note each producer takes the tail with an exchange and publishes its own
   *  node by linking it, so the value a node carries is only ever written by
   *  the thread that linked it
   */
  class MpBack {
  public:
    MpBack() PF_NOEXCEPT = default;
    MpBack(const ObjectStorage<storage_type>& spareStorage)
      : m_back(pointer_cast<Node*>(spareStorage.data)) {
      PF_REQUIRE(spareStorage.size == 1);
    }

    ~MpBack() PF_NOEXCEPT = default;

    MpBack(const MpBack&) = delete;
    MpBack&
    operator=(const MpBack&) = delete;

    MpBack(MpBack&& other) PF_NOEXCEPT
      : m_back(other.m_back.exchange(nullptr, std::memory_order_acq_rel)) {}

    MpBack&
    operator=(MpBack&& other) PF_NOEXCEPT {
      if (this != &other) {
        PF_REQUIRE_ASSUME(isNull_());
        Node* const otherBack = other.m_back.exchange(nullptr, std::memory_order_acq_rel);
        m_back.store(otherBack, std::memory_order_release);
      }
      return *this;
    }

    [[nodiscard]] Node*
    link(Node* newNode) PF_NOEXCEPT {
      PF_REQUIRE_ASSUME(!isNull_());
      Node* const prevNode = m_back.exchange(newNode, std::memory_order_acq_rel);
      prevNode->next.store(newNode, std::memory_order_release);
      return newNode;
    }

    void
    makeNull() PF_NOEXCEPT {
      m_back.store(nullptr, std::memory_order_release);
    }

  private:
    [[nodiscard]] bool
    isNull_(const std::memory_order& order = std::memory_order_acquire) const
        PF_NOEXCEPT {
      return m_back.load(order) == nullptr;
    }

    std::atomic<Node*> m_back{nullptr};
  };

  template <class T_Front, class T_Back>
  class Skeleton {
  public:
    TRAITS;

    PF_CONTAINERS_INHERIT_TRAITS(Traits);

    Skeleton() PF_NOEXCEPT = default;

    Skeleton(const Skeleton&) = delete;
    Skeleton&
    operator=(const Skeleton&) = delete;

    Skeleton(Skeleton&&) PF_NOEXCEPT = default;
    Skeleton&
    operator=(Skeleton&&) PF_NOEXCEPT = default;

    ~Skeleton() PF_NOEXCEPT = default;

    Skeleton(const ObjectStorage<storage_type>& storage) PF_NOEXCEPT : m_front(storage),
                                                                       m_back(storage) {}

    [[nodiscard]] bool
    empty() const PF_NOEXCEPT {
      return m_front.empty();
    }

    [[nodiscard]] bool
    isNull() const PF_NOEXCEPT {
      return m_front.isNull();
    }

    /**
     *@brief links a node holding a value, the storage must not be
     *  owned by the queue, a node handed out by pop is a valid choice
     *
     *@returns the node that was linked
     */
    template <class... V_Args>
    Node*
    emplace(const ObjectStorage<storage_type>& storage, V_Args&&... args)
        PF_NOEXCEPT_COND(template Traits::is_nothrow_construct_v) {
      PF_REQUIRE_ASSUME(storage.size == 1 && !m_front.isNull());

      return m_back.link(std::construct_at(
          pointer_cast<Node*>(storage.data), nullptr, std::forward<V_Args>(args)...));
    }

    void
    push(const ObjectStorage<storage_type>& storage, T&& val)
        PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
      static_cast<void>(emplace(storage, std::forward<T>(val)));
    }

    void
    push(const ObjectStorage<storage_type>& storage, const T& val)
        PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
      static_cast<void>(emplace(storage, val));
    }

    /**
     *@brief removes the node at the front of the queue, the value it holds
     *  is owned by the caller from then on
     *
     *@returns nullptr when the queue is empty
     */
    [[nodiscard]] Node*
    pop() PF_NOEXCEPT {
      return m_front.pop();
    }

    /**
     *@brief releases the head, it is a spare, the queue must be
     *  drained first
     *
     *@note the spare must be popped before the queue is destroyed
     *
     *@returns nullptr once the spare has been popped
     */
    [[nodiscard]] Node*
    popSpare() PF_NOEXCEPT {
      if (m_front.isNull()) {
        return nullptr;
      }

      m_back.makeNull();
      return m_front.popSpare();
    }

  private:
    PF_CACHE_LINE_ALIGN_VAR
    T_Front m_front;
    PF_CACHE_LINE_ALIGN_VAR
    T_Back m_back;
  };

public:
  using SPSC = Skeleton<SpFront, SpBack>;
  using MPSC = Skeleton<SpFront, MpBack>;
};

}

export namespace adapters {

/**
 *@brief a queue data class, represented via a linked list
 *  under the hood, intended for multi threaded use with one
 *  producer and one consumer
 *
 *@note A moved from, or default constructed object must be
 *  initialized by one of the move operations before use.
 *
 *@note The spare, the head, must be popped with popSpare before
 *  destruction.
 *
 */
template <typename T>
using SPSCLLQueue = detail::ConcurrentLLQueue<T>::SPSC;

/**
 *@brief a queue data class, represented via a linked list
 *  under the hood, intended for multi threaded use with
 *  multiple producers and a single consumer
 *
 *@note A moved from, or default constructed object must be
 *  initialized by one of the move operations before use.
 *
 *@note The spare, the head, must be popped with popSpare before
 *  destruction.
 *
 */
template <typename T>
using MPSCLLQueue = detail::ConcurrentLLQueue<T>::MPSC;
}

}
