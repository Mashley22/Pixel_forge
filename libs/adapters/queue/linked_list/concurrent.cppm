module;

#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

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

private:
  class MpFront {
  public:
    MpFront() PF_NOEXCEPT = default;
    MpFront(const ObjectStorage<storage_type>& dummyStorage)
      : m_front(pointer_cast<Node*>(dummyStorage.data)) {
      PF_REQUIRE(dummyStorage.size == 1);
    }

    MpFront(const MpFront&) = delete;
    MpFront&
    operator=(const MpFront&) = delete;

    MpFront(MpFront&& other) PF_NOEXCEPT
      : m_front(other.m_front.exchange(nullptr, std::memory_order_relaxed)) {}
    MpFront&
    operator=(MpFront&& other) PF_NOEXCEPT {
      if (this != &other) {
        clear_();
        Node* const other_front =
            other.m_front.exchange(nullptr, std::memory_order::acq_rel);
        m_front.store(other_front, std::memory_order::acquire);
      }
      return *this;
    }

    ~MpFront() PF_NOEXCEPT { clear_(); }

    [[nodiscard]] Node*
    pop() PF_NOEXCEPT {
      PF_REQUIRE_ASSUME(!isNull());
      Node* const dummyNode{m_front.load(std::memory_order_acquire)};
      Node* next = dummyNode->next.load(std::memory_order_acquire);

      if (next == nullptr) {
        return nullptr;
      }

      while (!m_front.compare_exchange_weak(
          dummyNode, next, std::memory_order::relaxed, std::memory_order::acquire)) {
        next = dummyNode->next.load(std::memory_order::acquire);
        if (next == nullptr) {
          return nullptr;
        }
      }

      std::construct_at(&dummyNode->val, std::move(next->val));
      std::destroy_at(&next->val);

      return dummyNode;
    }

    [[nodiscard]] bool
    isNull(const std::memory_order& order = std::memory_order_acquire) const PF_NOEXCEPT {
      return m_front.load(order) == nullptr;
    }

    [[nodiscard]] bool
    empty(const std::memory_order& order = std::memory_order_acquire) const PF_NOEXCEPT {
      return m_front.load(order)->next.load(order) == nullptr;
    }

    [[nodiscard]] Node*
    popDummy(const std::memory_order& order = std::memory_order_acq_rel) PF_NOEXCEPT {
      PF_REQUIRE_ASSUME(!isNull() && empty());
      return m_front.exchange(nullptr, order);
    }

  private:
    std::atomic<Node*> m_front{nullptr};

    void
    clear_() PF_NOEXCEPT {
      PF_REQUIRE_ASSUME(
          isNull(),
          "linked list queue adapter must be cleaned up before destructor is called"
          "it should be used as part of a class that empties the queue before "
          "destruction");
    }
  };

  class MpBack {
  public:
    MpBack() PF_NOEXCEPT = default;
    MpBack(const ObjectStorage<storage_type>& dummyStorage)
      : m_back(pointer_cast<Node*>(dummyStorage.data)) {
      PF_REQUIRE(dummyStorage.size == 1);
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
        Node* const other_back =
            other.m_back.exchange(nullptr, std::memory_order::acq_rel);
        m_back.store(other_back, std::memory_order_release);
      }
      return *this;
    }

    template <class... V_Args>
    value_type&
    emplace(const ObjectStorage<storage_type>& storage, V_Args&&... args)
        PF_NOEXCEPT_COND(template Traits::is_nothrow_construct_v) {
      PF_REQUIRE_ASSUME((storage.size == 1) && !isNull_());

      Node* const newNode = std::construct_at(
          pointer_cast<Node*>(storage.data), nullptr, std::forward<V_Args>(args)...);

      Node* const prevNode = m_back.exchange(newNode, std::memory_order_relaxed);
      prevNode->next.store(newNode, std::memory_order_relaxed);
      return newNode->val;
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

    void
    makeNull() PF_NOEXCEPT {
      m_back.store(nullptr, std::memory_order_acq_rel);
    }

  private:
    [[nodiscard]] bool
    isNull_(const std::memory_order& order = std::memory_order_acquire) PF_NOEXCEPT {
      return m_back.load(order) == nullptr;
    }

    std::atomic<Node*> m_back{nullptr};
  };

  class SpFront {
  public:
    SpFront() PF_NOEXCEPT = default;
    SpFront(const ObjectStorage<storage_type>& dummyStorage)
      : m_front(pointer_cast<Node*>(dummyStorage.data)) {
      PF_REQUIRE(dummyStorage.size == 1);
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
      return m_front->next.load(std::memory_order_acquire) == nullptr;
    }

    [[nodiscard]] Node*
    pop() PF_NOEXCEPT {
      PF_REQUIRE_ASSUME(!isNull());
      Node* const dummyNode{m_front};
      Node* const next = dummyNode->next.load(std::memory_order_acquire);
      if (next == nullptr) {
        return nullptr;
      }

      std::construct_at(&dummyNode->val, std::move(next->val));
      std::destroy_at(&next->val);
      m_front = next;

      return dummyNode;
    }

    [[nodiscard]] Node*
    popDummy() PF_NOEXCEPT {
      PF_REQUIRE_ASSUME(!isNull() && empty());
      Node* const dummy = m_front;
      m_front = nullptr;
      return dummy;
    }

  private:
    Node* m_front{nullptr};

    void
    clear_() PF_NOEXCEPT {
      PF_REQUIRE_ASSUME(
          isNull(),
          "linked list queue adapter must be cleaned up before destructor is called"
          "it should be used as part of a class that empties the queue before "
          "destruction");
    }
  };

  class SpBack {
  public:
    SpBack() PF_NOEXCEPT = default;
    SpBack(const ObjectStorage<storage_type>& dummyStorage)
      : m_back(pointer_cast<Node*>(dummyStorage.data)) {
      PF_REQUIRE(dummyStorage.size == 1);
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

    template <class... V_Args>
    void
    emplace(const ObjectStorage<storage_type>& storage, V_Args&&... args)
        PF_NOEXCEPT_COND(template Traits::is_nothrow_construct_v) {
      PF_REQUIRE_ASSUME(storage.size == 1 && !isNull_());

      Node* newNode = std::construct_at(
          pointer_cast<Node*>(storage.data), nullptr, std::forward<V_Args>(args)...);

      m_back->next.store(newNode, std::memory_order_release);
      m_back = NonNull<Node*>::from(newNode);
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

  template <class T_Front, class T_Back>
  class Skeleton {
  public:
    TRAITS;

    PF_ADAPTERS_INHERIT_TRAITS(Traits);

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

    template <class... V_Args>
    void
    emplace(const ObjectStorage<storage_type>& storage, V_Args&&... args)
        PF_NOEXCEPT_COND(template Traits::is_nothrow_construct_v) {
      return m_back.emplace(storage, std::forward<V_Args>(args)...);
    }

    void
    push(const ObjectStorage<storage_type>& storage, T&& val)
        PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
      return emplace(storage, std::forward<T>(val));
    }

    void
    push(const ObjectStorage<storage_type>& storage, const T& val)
        PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
      return emplace(storage, val);
    }

    Node*
    pop() PF_NOEXCEPT {
      return m_front.pop();
    }

    Node*
    popDummy() PF_NOEXCEPT {
      m_back.makeNull();
      return m_front.popDummy();
    }

  private:
    PF_CACHE_LINE_ALIGN_VAR
    T_Front m_front;
    PF_CACHE_LINE_ALIGN_VAR
    T_Back m_back;
  };

public:
  using SPSC = Skeleton<SpFront, SpBack>;
  using SPMC = Skeleton<MpFront, SpBack>;
  using MPSC = Skeleton<SpFront, MpBack>;
  using MPMC = Skeleton<MpFront, MpBack>;
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
using SPSCLLQueue = detail::ConcurrentLLQueue<T>::SPSC;

template <typename T>
using MPSCLLQueue = detail::ConcurrentLLQueue<T>::MPSC;

template <typename T>
using SPSMLLQueue = detail::ConcurrentLLQueue<T>::SPMC;

template <typename T>
using MPMCLLQueue = detail::ConcurrentLLQueue<T>::MPMC;

}

}
