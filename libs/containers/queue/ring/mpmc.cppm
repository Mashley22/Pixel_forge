module;

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>

#include <PixelForge/containers/macros.hpp>
#include <PixelForge/core/macros.hpp>

export module PixelForge.containers:mpmcRingQueue;

import :utils.traits;

import PixelForge.core;

export namespace pf::adapters {

/**
 *@brief a bounded queue, represented via a ring, intended for multi
 *  threaded use with multiple producers and multiple consumers
 *
 *@note a thread is handed a ticket before it touches a slot and the
 *  committed cursors only ever advance in ticket order, so a slot is
 *  never written before the slot ahead of it, nor read before it has
 *  been written
 *
 *@note publishing a slot is a release on the write committed cursor, a
 *  consumer acquires it before it reads the slot, handing a slot back is
 *  a release on the read committed cursor and a producer acquires it
 *  before it writes the slot, which is what keeps a producer from
 *  overwriting a slot a consumer is still reading
 *
 *@note a ticket is never handed back once taken, so a ticket is always
 *  committed, even when the queue is closed half way through
 *
 *@note the try_ operations admit a single operation at a time per side
 *  and report failure when another thread is midway through one, the
 *  wait_ operations let them queue up instead and block until it is
 *  their turn
 *
 *@note the queue must be drained before it is destroyed
 */
template <typename T>
class MPMCRingQueue {
public:
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
    template <typename... V_args>
    static constexpr bool is_nothrow_construct_v =
        std::is_nothrow_constructible_v<T, V_args...>;
  };

  PF_CONTAINERS_INHERIT_TRAITS(Traits);

  /**
   *@brief Constructs a queue over a typed ObjectStorage
   *
   * The storage's data pointer must be aligned for @p T and its size must be
   * a power of two. The storage holds raw storage, the queue constructs and
   * destroys the values in it, and it must outlive the queue.
   */
  explicit MPMCRingQueue(ObjectStorage<storage_type> storage) PF_NOEXCEPT
    : m_data(pointer_cast<pointer>(storage.data)),
      m_mask(storage.size - 1) {
    PF_REQUIRE(valid_init_());
  }

  ~MPMCRingQueue() PF_NOEXCEPT {
    PF_REQUIRE(empty(), "MPMC ring queue should be drained before destruction");
  }

  MPMCRingQueue(const MPMCRingQueue&) = delete;
  MPMCRingQueue(MPMCRingQueue&&) = delete;
  MPMCRingQueue&
  operator=(const MPMCRingQueue&) = delete;
  MPMCRingQueue&
  operator=(MPMCRingQueue&&) = delete;

  [[nodiscard]] constexpr size_type
  capacity() const PF_NOEXCEPT {
    return m_mask + 1;
  }

  /**
   *@brief the number of values that have been written to the queue but do not have
   * a read reserved
   */
  [[nodiscard]] size_type
  size() const PF_NOEXCEPT {
    // the consumed cursor is read first, the produced one can only have grown
    // since, so the subtraction never underflows
    const size_type consumed = m_read.reserved.load(std::memory_order_acquire);
    const size_type produced = m_write.committed.load(std::memory_order_acquire);
    return produced - consumed;
  }

  [[nodiscard]] size_type
  remaining() const PF_NOEXCEPT {
    return capacity() - size();
  }

  [[nodiscard]] bool
  empty() const PF_NOEXCEPT {
    return size() == 0;
  }

  /**
   *@brief true when no slot is definitely free
   *
   *@note a slot that has been reserved by a producer but not yet written is
   *  counted as taken, so this can be true while \ref size is smaller than
   *  the capacity
   */
  [[nodiscard]] bool
  full() const PF_NOEXCEPT {
    const size_type consumed = m_read.committed.load(std::memory_order_acquire);
    const size_type reserved = m_write.reserved.load(std::memory_order_acquire);
    return (reserved - consumed) >= capacity();
  }

  /**
   *@brief constructs a value in the next slot
   *
   *@returns false when the queue is closed, full, or another thread is
   *  midway through a push
   */
  template <class... V_Args>
  bool
  try_emplace(V_Args&&... args)
      PF_NOEXCEPT_COND(template Traits::is_nothrow_construct_v) {

    const size_type consumed = m_read.committed.load(std::memory_order_acquire);
    const size_type committed = m_write.committed.load(std::memory_order_acquire);
    size_type reserved = m_write.reserved.load(std::memory_order_relaxed);

    // a producer that is already writing is the only way the committed cursor
    // can lag the reserved one, and committing out of ticket order would let a
    // consumer read a slot that has not been written
    if ((reserved - consumed) >= capacity() || reserved != committed) {
      return false;
    }

    if (!m_write.reserved.compare_exchange_weak(reserved,
                                                reserved + 1,
                                                std::memory_order_acq_rel,
                                                std::memory_order_relaxed)) {
      return false;
    }

    // taking the ticket makes this the only push in flight, so this thread is
    // the one that advances the committed cursor
    std::construct_at(&m_data[idx_(reserved)], std::forward<V_Args>(args)...);
    m_write.committed.fetch_add(1, std::memory_order_release);

    return true;
  }

  /**
   *@brief constructs a value in the next slot, waiting for a free slot and
   *  for the turn of the threads ahead
   *
   *@returns false when the queue is closed while waiting
   */
  template <class... V_Args>
  bool
  wait_emplace(V_Args&&... args)
      PF_NOEXCEPT_COND(template Traits::is_nothrow_construct_v) {
    size_type reserved = 0;

    // taking the ticket and waiting for the tickets ahead are two phases, a
    // thread that has a ticket must never come back here and take another
    for (;;) {

      // the event counter is read before the cursors are, so a signal that
      // lands in between leaves it changed and the wait returns straight away
      const size_type consumed = m_read.committed.load(std::memory_order_acquire);
      reserved = m_write.reserved.load(std::memory_order_relaxed);
      if ((reserved - consumed) >= capacity()) {
        // no slot is free, a consumer has to finish reading one before it can
        // be handed back
        spinUntil_(
            [&] { return m_read.committed.load(std::memory_order_acquire) != consumed; });
        continue;
      }

      if (m_write.reserved.compare_exchange_weak(reserved,
                                                 reserved + 1,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_relaxed)) {
        break;
      }
    }

    // the slot can only be written once every ticket ahead of this one has
    // been written. The cursor waited on is the one that moves, so waiting on
    // the value it was read at cannot miss the change that releases it
    for (;;) {
      const size_type cur = m_write.committed.load(std::memory_order_acquire);
      if (cur == reserved) {
        break;
      }
      spinUntil_(
          [&]() { return m_write.committed.load(std::memory_order_acquire) != cur; });
    }

    std::construct_at(&m_data[idx_(reserved)], std::forward<V_Args>(args)...);
    m_write.committed.store(reserved + 1, std::memory_order_release);

    return true;
  }

  /**
   *@brief removes the value at the front of the queue
   *
   *@returns std::nullopt when the queue is empty or another thread is
   *  midway through a pop
   */
  [[nodiscard]] std::optional<value_type>
  try_pop() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    const size_type produced = m_write.committed.load(std::memory_order_acquire);
    const size_type committed = m_read.committed.load(std::memory_order_acquire);
    size_type reserved = m_read.reserved.load(std::memory_order_relaxed);

    // a slot that is reserved but not yet written cannot be read, and
    // committing out of ticket order would hand the same slot out twice
    if (produced <= reserved || reserved != committed) {
      return std::nullopt;
    }

    if (!m_read.reserved.compare_exchange_weak(reserved,
                                               reserved + 1,
                                               std::memory_order_acq_rel,
                                               std::memory_order_relaxed)) {
      return std::nullopt;
    }

    // taking the ticket makes this the only pop in flight, so this thread is
    // the one that hands the slot back
    std::optional<value_type> popped{std::move(m_data[idx_(reserved)])};
    std::destroy_at(&m_data[idx_(reserved)]);
    m_read.committed.fetch_add(1, std::memory_order_release);

    return popped;
  }

  /**
   *@brief removes the value at the front of the queue, waiting for a value
   *  and for the turn of the threads ahead
   *
   *@note a closed queue can still be drained, and reports itself drained
   *  with std::nullopt once it is
   */
  [[nodiscard]] std::optional<value_type>
  wait_pop() PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
    size_type reserved = 0;

    for (;;) {
      const size_type produced = m_write.committed.load(std::memory_order_acquire);
      reserved = m_read.reserved.load(std::memory_order_relaxed);
      if (produced <= reserved) {
        // nothing to read, either the queue is empty or a producer is midway
        // through writing a slot
        spinUntil_([&] {
          return m_write.committed.load(std::memory_order_acquire) != produced;
        });
        continue;
      }

      if (m_read.reserved.compare_exchange_weak(reserved,
                                                reserved + 1,
                                                std::memory_order_acq_rel,
                                                std::memory_order_relaxed)) {
        break;
      }
    }

    // the slot can only be read once every ticket ahead of this one has been
    // read, and the cursor waited on is the one that moves
    for (;;) {
      const size_type cur = m_read.committed.load(std::memory_order_acquire);
      if (cur == reserved) {
        break;
      }
      spinUntil_(
          [&]() { return m_read.committed.load(std::memory_order_acquire) != cur; });
    }

    std::optional<value_type> popped{std::move(m_data[idx_(reserved)])};
    std::destroy_at(&m_data[idx_(reserved)]);
    m_read.committed.store(reserved + 1, std::memory_order_release);

    return popped;
  }

  template <class... V_Args>
  bool
  try_push(V_Args&&... args) PF_NOEXCEPT_COND(template Traits::is_nothrow_construct_v) {
    return try_emplace(std::forward<V_Args>(args)...);
  }

  template <class... V_Args>
  bool
  wait_push(V_Args&&... args) PF_NOEXCEPT_COND(template Traits::is_nothrow_construct_v) {
    return wait_emplace(std::forward<V_Args>(args)...);
  }

private:
  /**
   *@brief a pair of cursors, the reserved one is handed out ticket by
   *  ticket, the committed one only advances once the ticket ahead of it
   *  has been dealt with
   */
  struct Cursors {
    PF_CACHE_LINE_ALIGN_VAR std::atomic<size_type> reserved{0};
    PF_CACHE_LINE_ALIGN_VAR std::atomic<size_type> committed{0};
  };

  using spin_t = std::uint32_t;
  static constexpr spin_t SPIN_LIMIT = 65536;

  /**
   *@brief re-checks a predicate a bounded number of times before a thread
   *  parks, a short spin is cheaper than a park and covers the case where a
   *  signal is already in flight
   *
   *@returns true if the predicate held, in which case the caller should
   *  re-evaluate everything
   */
  template <class T_Predicate>
  void
  spinUntil_(T_Predicate&& predicate) PF_NOEXCEPT {
    for (spin_t i = 0; i < SPIN_LIMIT; ++i) {
      if (predicate()) {
        return;
      }
      constexpr spin_t randomNum = 128;
      if ((i & randomNum) == randomNum) {
        std::this_thread::yield();
      }
    }
    return;
  }

  [[nodiscard]] constexpr size_type
  idx_(size_type num) const PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(m_data != nullptr && m_mask > 0);
    return num & m_mask;
  }

  [[nodiscard]] constexpr bool
  valid_init_() const PF_NOEXCEPT {
    return m_data != nullptr && capacity() > 0 && std::has_single_bit(capacity());
  }

  pointer m_data{nullptr};
  size_type m_mask{0};

  PF_CACHE_LINE_ALIGN_VAR Cursors m_read;
  PF_CACHE_LINE_ALIGN_VAR Cursors m_write;
};

}
