module;

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

#include <PixelForge/core/macros.hpp>

export module PixelForge.mem:pool_arena;

import PixelForge.core;
import :align;

namespace pf::mem {

export class ConcurrentPoolArena {
public:
  using size_type = std::size_t;
  constexpr ConcurrentPoolArena() PF_NOEXCEPT = default;

  struct Slot {
    Buffer buffer;
    size_type blockIdx{};
    [[nodiscard]] constexpr bool
    isNull() const PF_NOEXCEPT {
      return buffer.isNull();
    }
  };

  struct CreateParams {
    Buffer buffer;
    size_type block_size{};
    size_type block_count{};
    size_type alignment{0};
  };

  ConcurrentPoolArena(const CreateParams& params)
    : m_freeHead(0), m_data(params.buffer.data), m_capMask(params.block_count - 1),
      m_alignment(std::max(params.alignment, alignof(size_type))),
      m_stride(std::max(align(params.block_size, alignment()), sizeof(size_type))),
      m_userBlockSize(params.block_size) PF_NOEXCEPT {
    PF_REQUIRE(std::has_single_bit(params.block_size));
    PF_REQUIRE(std::has_single_bit(params.block_count));
    PF_REQUIRE(pointer_cast<std::uintptr_t>(params.buffer.data) % alignment() == 0);
    PF_REQUIRE(params.buffer.size == params.block_count * m_stride);
    initialiseFreeList_();
  }

  ConcurrentPoolArena(const ConcurrentPoolArena&) = delete;
  ConcurrentPoolArena&
  operator=(const ConcurrentPoolArena&) = delete;

  ConcurrentPoolArena(ConcurrentPoolArena&& other) PF_NOEXCEPT
    : m_freeHead(other.m_freeHead.exchange(null, std::memory_order::seq_cst)),
      m_data(std::exchange(other.m_data, nullptr)),
      m_capMask(std::exchange(other.m_capMask, 0)),
      m_alignment(other.m_alignment),
      m_stride(other.m_stride),
      m_userBlockSize(other.m_userBlockSize) {}

  ConcurrentPoolArena&
  // NOLINTNEXTLINE(bugprone-exception-escape)
  operator=(ConcurrentPoolArena&& other) PF_NOEXCEPT {
    if (this != &other) {
      PF_REQUIRE(isNull());
      m_data = std::exchange(other.m_data, nullptr);
      m_capMask = std::exchange(other.m_capMask, 0);
      m_stride = other.m_stride;
      m_alignment = other.m_alignment;
      m_userBlockSize = other.m_userBlockSize;
      m_freeHead = other.m_freeHead.exchange(null, std::memory_order::seq_cst);
    }
    return *this;
  }

  ~ConcurrentPoolArena() PF_NOEXCEPT = default;

  [[nodiscard]] constexpr size_type
  blockCapacity() const PF_NOEXCEPT {
    PF_REQUIRE(!isNull(), "a null arena has no capacity");
    return m_capMask + 1;
  }

  [[nodiscard]] constexpr size_type
  capacity() const PF_NOEXCEPT {
    PF_REQUIRE(!isNull(), "a null arena has no capacity");
    return blockCapacity() * stride();
  }

  [[nodiscard]] constexpr bool
  full(const std::memory_order& order = std::memory_order::acquire) const PF_NOEXCEPT {
    return m_freeHead.load(order) == null;
  }

  [[nodiscard]] constexpr bool
  isNull() const PF_NOEXCEPT {
    return m_data == nullptr;
  }

  [[nodiscard]] constexpr size_type
  alignment() const PF_NOEXCEPT {
    return m_alignment;
  }

  [[nodiscard]] constexpr size_type
  stride() const PF_NOEXCEPT {
    return m_stride;
  }

  [[nodiscard]] constexpr size_type
  blockSize() const PF_NOEXCEPT {
    return m_userBlockSize;
  }

  [[nodiscard]] constexpr const std::byte*
  resolve(size_type blockIdx) const PF_NOEXCEPT {
    PF_REQUIRE(blockIdx < blockCapacity());
    return &m_data[blockIdx * stride()];
  }

  [[nodiscard]] constexpr std::byte*
  resolve(size_type blockIdx) PF_NOEXCEPT {
    PF_REQUIRE(blockIdx < blockCapacity());
    return &m_data[blockIdx * stride()];
  }

  [[nodiscard]] constexpr const std::byte*
  resolve(const Slot& slot) const PF_NOEXCEPT {
    return resolve(slot.blockIdx);
  }

  [[nodiscard]] constexpr std::byte*
  resolve(const Slot& slot) PF_NOEXCEPT {
    return resolve(slot.blockIdx);
  }

  /**
   * @brief Allocates a slot in the pool, returning the pointer to the allocated
   * memory, the buffer size is the stride
   *
   *
   */
  [[nodiscard]] Slot
  alloc() PF_NOEXCEPT {
    size_type head = m_freeHead.load(std::memory_order_acquire);

    for (;;) {
      if (head == null) {
        return Slot{.buffer = Buffer::null()};
      }

      const size_type headBlockIdx = indexOf_(head);
      const size_type nextBlockIdx = getNextBlockIdx_(headBlockIdx);
      const size_type generation = generationOf_(head);
      const size_type nextHead =
          nextBlockIdx == null ? null : tagIdx_(generation, nextBlockIdx);

      if (m_freeHead.compare_exchange_weak(head, nextHead, std::memory_order_acq_rel)) {
        std::destroy_at(pointer_cast<size_type*>(&m_data[headBlockIdx * stride()]));
        return Slot{.buffer{.data = resolve(headBlockIdx), .size = m_userBlockSize},
                    .blockIdx = headBlockIdx};
      }
    }
  }

  void
  dealloc(size_type blockIdx) PF_NOEXCEPT {
    size_type head = m_freeHead.load(std::memory_order_acquire);
    size_type* const ptr = pointer_cast<size_type*>(resolve(blockIdx));
    for (;;) {
      std::construct_at(ptr, head == null ? null : indexOf_(head));
      const size_type generation = head == null ? 0 : generationOf_(head);
      if (m_freeHead.compare_exchange_weak(head,
                                           tagIdx_(bumpGeneration_(generation), blockIdx),
                                           std::memory_order_acq_rel)) {
        return;
      }
    }
  }

  void
  dealloc(std::byte* ptr) PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(
        (pointer_cast<std::uintptr_t>(ptr) - pointer_cast<std::uintptr_t>(m_data)) %
            m_stride ==
        0);
    const size_type blockIdx =
        (pointer_cast<std::uintptr_t>(ptr) - pointer_cast<std::uintptr_t>(m_data)) /
        m_stride;

    dealloc(blockIdx);
  }

private:
  static constexpr size_type null = std::numeric_limits<size_type>::max();

  PF_CACHE_LINE_ALIGN_VAR
  std::atomic<size_type> m_freeHead{null};

  std::byte* m_data{nullptr};
  size_type m_capMask{0}; // in units of the stride
  size_type m_alignment{0};
  size_type m_stride{0};
  size_type m_userBlockSize{0};

  constexpr void
  initialiseFreeList_() PF_NOEXCEPT {
    for (size_type i = 0; i < blockCapacity(); i++) {
      const size_type nextIdx = i + 1 == blockCapacity() ? null : i + 1;
      std::construct_at(pointer_cast<size_type*>(&m_data[i * stride()]), nextIdx);
    }
  }

  constexpr size_type
  getNextBlockIdx_(size_type blockIdx) const PF_NOEXCEPT {
    size_type nextIdx{0};
    std::memcpy(&nextIdx, &m_data[blockIdx * stride()], sizeof(size_type));
    return nextIdx;
  }

  [[nodiscard]] constexpr size_type
  indexOf_(size_type tagged) const PF_NOEXCEPT {
    return tagged & m_capMask;
  }

  [[nodiscard]] constexpr size_type
  generationOf_(size_type tagged) const PF_NOEXCEPT {
    return (tagged & ~m_capMask);
  }

  [[nodiscard]] constexpr size_type
  tagIdx_(size_type generation, size_type index) const PF_NOEXCEPT {
    return (generation) | (index & m_capMask);
  }

  [[nodiscard]] constexpr size_type
  bumpGeneration_(size_type generation) const PF_NOEXCEPT {
    return generation + blockCapacity();
  }
};

export class PoolArena {
public:
  using size_type = std::size_t;
  constexpr PoolArena() PF_NOEXCEPT = default;

  struct CreateParams {
    Buffer buffer;
    size_type block_size{};
    size_type block_count{};
    size_type alignment{0};
  };

  PoolArena(const CreateParams& params)
    : m_data(params.buffer.data), m_blockCapacity(params.block_count),
      m_alignment(std::max(params.alignment, alignof(void*))),
      m_stride(std::max(align(params.block_size, alignment()), sizeof(size_type))),
      m_userBlockSize(params.block_size) PF_NOEXCEPT {
    PF_REQUIRE(std::has_single_bit(params.block_size));
    PF_REQUIRE(std::has_single_bit(params.block_count));
    PF_REQUIRE(pointer_cast<std::uintptr_t>(params.buffer.data) % alignment() == 0);
    PF_REQUIRE(params.buffer.size == params.block_count * m_stride);
    initialiseFreeList_();
  }

  PoolArena(const PoolArena&) = delete;
  PoolArena&
  operator=(const PoolArena&) = delete;

  PoolArena(PoolArena&& other) PF_NOEXCEPT
    : m_freeHead(std::exchange(other.m_freeHead, nullptr)),
      m_data(std::exchange(other.m_data, nullptr)),
      m_blockCapacity(std::exchange(other.m_blockCapacity, 0)),
      m_alignment(other.m_alignment),
      m_stride(other.m_stride),
      m_userBlockSize(other.m_userBlockSize) {}

  PoolArena&
  // NOLINTNEXTLINE(bugprone-exception-escape)
  operator=(PoolArena&& other) PF_NOEXCEPT {
    if (this != &other) {
      PF_REQUIRE(isNull());
      m_data = std::exchange(other.m_data, nullptr);
      m_blockCapacity = std::exchange(other.m_blockCapacity, 0);
      m_stride = other.m_stride;
      m_alignment = other.m_alignment;
      m_userBlockSize = other.m_userBlockSize;
      m_freeHead = other.m_freeHead;
    }
    return *this;
  }

  ~PoolArena() PF_NOEXCEPT = default;

  [[nodiscard]] constexpr bool
  full() const PF_NOEXCEPT {
    return m_freeHead == nullptr;
  }

  [[nodiscard]] constexpr bool
  isNull() const PF_NOEXCEPT {
    return m_data == nullptr;
  }

  [[nodiscard]] constexpr size_type
  alignment() const PF_NOEXCEPT {
    return m_alignment;
  }

  [[nodiscard]] constexpr size_type
  stride() const PF_NOEXCEPT {
    return m_stride;
  }

  [[nodiscard]] constexpr size_type
  blockSize() const PF_NOEXCEPT {
    return m_userBlockSize;
  }

  [[nodiscard]] constexpr size_type
  blockCapacity() const PF_NOEXCEPT {
    return m_blockCapacity;
  }

  [[nodiscard]] constexpr size_type
  capacity() const PF_NOEXCEPT {
    return blockCapacity() * stride();
  }

  /**
   * @brief Allocates a slot in the pool, returning the pointer to the allocated
   * memory, the buffer size is the requested block size
   *
   *
   */
  [[nodiscard]] Buffer
  alloc() PF_NOEXCEPT {
    if (m_freeHead == nullptr) {
      return Buffer{.data = nullptr, .size = 0};
    }
    Link* oldHead = std::exchange(m_freeHead, m_freeHead->next);
    std::destroy_at(oldHead);
    return Buffer{.data = pointer_cast<std::byte*>(oldHead), .size = m_userBlockSize};
  }

  void
  dealloc(std::byte* ptr) PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(
        (pointer_cast<std::uintptr_t>(ptr) - pointer_cast<std::uintptr_t>(m_data)) %
            stride() ==
        0);

    PF_REQUIRE_ASSUME(pointer_cast<std::uintptr_t>(ptr) -
                          pointer_cast<std::uintptr_t>(m_data) <
                      capacity());
    m_freeHead = std::construct_at(pointer_cast<Link*>(ptr), Link{.next = m_freeHead});
  }

private:
  struct Link {
    Link* next{nullptr};
  };

  Link* m_freeHead{nullptr};
  std::byte* m_data{nullptr};
  size_type m_blockCapacity{0};
  size_type m_alignment{0};
  size_type m_stride{0};
  size_type m_userBlockSize{0};

  constexpr void
  initialiseFreeList_() PF_NOEXCEPT {
    Link* head{nullptr};
    for (std::size_t i = blockCapacity(); i > 0; i--) {
      head = std::construct_at(pointer_cast<Link*>(&m_data[(i - 1) * stride()]),
                               Link{.next = head});
    }
    m_freeHead = head;
  }
};

}
