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
  using SizeType = std::size_t;
  constexpr ConcurrentPoolArena() PF_NOEXCEPT = default;

  struct Slot {
    Buffer buffer;
    SizeType block_idx{};
    [[nodiscard]] constexpr bool
    is_null() const PF_NOEXCEPT {
      return buffer.is_null();
    }
  };

  struct CreateParams {
    Buffer buffer;
    SizeType block_size{};
    SizeType block_count{};
    SizeType alignment{0};
  };

  ConcurrentPoolArena(const CreateParams& params)
    : m_free_head(0), m_data(params.buffer.data), m_cap_mask(params.block_count - 1),
      m_alignment(std::max(params.alignment, alignof(SizeType))),
      m_stride(std::max(align(params.block_size, alignment()), sizeof(SizeType))),
      m_user_block_size(params.block_size) PF_NOEXCEPT {
    PF_REQUIRE(std::has_single_bit(params.block_size));
    PF_REQUIRE(std::has_single_bit(params.block_count));
    PF_REQUIRE(pointer_cast<std::uintptr_t>(params.buffer.data) % alignment() == 0);
    PF_REQUIRE(params.buffer.size == params.block_count * m_stride);
    initialise_free_list();
  }

  ConcurrentPoolArena(const ConcurrentPoolArena&) = delete;
  ConcurrentPoolArena&
  operator=(const ConcurrentPoolArena&) = delete;

  ConcurrentPoolArena(ConcurrentPoolArena&& other) PF_NOEXCEPT
    : m_free_head(other.m_free_head.exchange(null, std::memory_order::seq_cst)),
      m_data(std::exchange(other.m_data, nullptr)),
      m_cap_mask(std::exchange(other.m_cap_mask, 0)),
      m_alignment(other.m_alignment),
      m_stride(other.m_stride),
      m_user_block_size(other.m_user_block_size) {}

  ConcurrentPoolArena&
  // NOLINTNEXTLINE(bugprone-exception-escape)
  operator=(ConcurrentPoolArena&& other) PF_NOEXCEPT {
    if (this != &other) {
      PF_REQUIRE(is_null());
      m_data = std::exchange(other.m_data, nullptr);
      m_cap_mask = std::exchange(other.m_cap_mask, 0);
      m_stride = other.m_stride;
      m_alignment = other.m_alignment;
      m_user_block_size = other.m_user_block_size;
      m_free_head = other.m_free_head.exchange(null, std::memory_order::seq_cst);
    }
    return *this;
  }

  ~ConcurrentPoolArena() PF_NOEXCEPT = default;

  [[nodiscard]] constexpr SizeType
  block_capacity() const PF_NOEXCEPT {
    PF_REQUIRE(!is_null(), "a null arena has no capacity");
    return m_cap_mask + 1;
  }

  [[nodiscard]] constexpr SizeType
  capacity() const PF_NOEXCEPT {
    PF_REQUIRE(!is_null(), "a null arena has no capacity");
    return block_capacity() * stride();
  }

  [[nodiscard]] constexpr bool
  full(const std::memory_order& order = std::memory_order::acquire) const PF_NOEXCEPT {
    return m_free_head.load(order) == null;
  }

  [[nodiscard]] constexpr bool
  is_null() const PF_NOEXCEPT {
    return m_data == nullptr;
  }

  [[nodiscard]] constexpr SizeType
  alignment() const PF_NOEXCEPT {
    return m_alignment;
  }

  [[nodiscard]] constexpr SizeType
  stride() const PF_NOEXCEPT {
    return m_stride;
  }

  [[nodiscard]] constexpr SizeType
  block_size() const PF_NOEXCEPT {
    return m_user_block_size;
  }

  [[nodiscard]] constexpr const std::byte*
  resolve(SizeType block_idx) const PF_NOEXCEPT {
    PF_REQUIRE(block_idx < block_capacity());
    return &m_data[block_idx * stride()];
  }

  [[nodiscard]] constexpr std::byte*
  resolve(SizeType block_idx) PF_NOEXCEPT {
    PF_REQUIRE(block_idx < block_capacity());
    return &m_data[block_idx * stride()];
  }

  [[nodiscard]] constexpr const std::byte*
  resolve(const Slot& slot) const PF_NOEXCEPT {
    return resolve(slot.block_idx);
  }

  [[nodiscard]] constexpr std::byte*
  resolve(const Slot& slot) PF_NOEXCEPT {
    return resolve(slot.block_idx);
  }

  /**
   * @brief Allocates a slot in the pool, returning the pointer to the allocated
   * memory, the buffer size is the stride
   *
   *
   */
  [[nodiscard]] Slot
  alloc() PF_NOEXCEPT {
    SizeType head = m_free_head.load(std::memory_order_acquire);

    for (;;) {
      if (head == null) {
        return Slot{.buffer = Buffer::null()};
      }

      const SizeType head_block_idx = index_of(head);
      const SizeType next_block_idx = get_next_block_idx(head_block_idx);
      const SizeType generation = generation_of(head);
      const SizeType next_head =
          next_block_idx == null ? null : tag_idx(generation, next_block_idx);

      if (m_free_head.compare_exchange_weak(head, next_head, std::memory_order_acq_rel)) {
        std::destroy_at(pointer_cast<SizeType*>(&m_data[head_block_idx * stride()]));
        return Slot{.buffer{.data = resolve(head_block_idx), .size = m_user_block_size},
                    .block_idx = head_block_idx};
      }
    }
  }

  void
  dealloc(SizeType block_idx) PF_NOEXCEPT {
    SizeType head = m_free_head.load(std::memory_order_acquire);
    SizeType* const ptr = pointer_cast<SizeType*>(resolve(block_idx));
    for (;;) {
      std::construct_at(ptr, head == null ? null : index_of(head));
      const SizeType generation = head == null ? 0 : generation_of(head);
      if (m_free_head.compare_exchange_weak(
              head,
              tag_idx(bump_generation(generation), block_idx),
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
    const SizeType block_idx =
        (pointer_cast<std::uintptr_t>(ptr) - pointer_cast<std::uintptr_t>(m_data)) /
        m_stride;

    dealloc(block_idx);
  }

private:
  static constexpr SizeType null = std::numeric_limits<SizeType>::max();

  PF_CACHE_LINE_ALIGN_VAR
  std::atomic<SizeType> m_free_head{null};

  std::byte* m_data{nullptr};
  SizeType m_cap_mask{0}; // in units of the stride
  SizeType m_alignment{0};
  SizeType m_stride{0};
  SizeType m_user_block_size{0};

  constexpr void
  initialise_free_list() PF_NOEXCEPT {
    for (SizeType i = 0; i < block_capacity(); i++) {
      const SizeType next_idx = i + 1 == block_capacity() ? null : i + 1;
      std::construct_at(pointer_cast<SizeType*>(&m_data[i * stride()]), next_idx);
    }
  }

  constexpr SizeType
  get_next_block_idx(SizeType block_idx) const PF_NOEXCEPT {
    SizeType next_idx{0};
    std::memcpy(&next_idx, &m_data[block_idx * stride()], sizeof(SizeType));
    return next_idx;
  }

  [[nodiscard]] constexpr SizeType
  index_of(SizeType tagged) const PF_NOEXCEPT {
    return tagged & m_cap_mask;
  }

  [[nodiscard]] constexpr SizeType
  generation_of(SizeType tagged) const PF_NOEXCEPT {
    return (tagged & ~m_cap_mask);
  }

  [[nodiscard]] constexpr SizeType
  tag_idx(SizeType generation, SizeType index) const PF_NOEXCEPT {
    return (generation) | (index & m_cap_mask);
  }

  [[nodiscard]] constexpr SizeType
  bump_generation(SizeType generation) const PF_NOEXCEPT {
    return generation + block_capacity();
  }
};

export class PoolArena {
public:
  using SizeType = std::size_t;
  constexpr PoolArena() PF_NOEXCEPT = default;

  struct CreateParams {
    Buffer buffer;
    SizeType block_size{};
    SizeType block_count{};
    SizeType alignment{0};
  };

  PoolArena(const CreateParams& params)
    : m_data(params.buffer.data), m_block_capacity(params.block_count),
      m_alignment(std::max(params.alignment, alignof(void*))),
      m_stride(std::max(align(params.block_size, alignment()), sizeof(SizeType))),
      m_user_block_size(params.block_size) PF_NOEXCEPT {
    PF_REQUIRE(std::has_single_bit(params.block_size));
    PF_REQUIRE(std::has_single_bit(params.block_count));
    PF_REQUIRE(pointer_cast<std::uintptr_t>(params.buffer.data) % alignment() == 0);
    PF_REQUIRE(params.buffer.size == params.block_count * m_stride);
    initialise_free_list();
  }

  PoolArena(const PoolArena&) = delete;
  PoolArena&
  operator=(const PoolArena&) = delete;

  PoolArena(PoolArena&& other) PF_NOEXCEPT
    : m_free_head(std::exchange(other.m_free_head, nullptr)),
      m_data(std::exchange(other.m_data, nullptr)),
      m_block_capacity(std::exchange(other.m_block_capacity, 0)),
      m_alignment(other.m_alignment),
      m_stride(other.m_stride),
      m_user_block_size(other.m_user_block_size) {}

  PoolArena&
  // NOLINTNEXTLINE(bugprone-exception-escape)
  operator=(PoolArena&& other) PF_NOEXCEPT {
    if (this != &other) {
      PF_REQUIRE(is_null());
      m_data = std::exchange(other.m_data, nullptr);
      m_block_capacity = std::exchange(other.m_block_capacity, 0);
      m_stride = other.m_stride;
      m_alignment = other.m_alignment;
      m_user_block_size = other.m_user_block_size;
      m_free_head = other.m_free_head;
    }
    return *this;
  }

  ~PoolArena() PF_NOEXCEPT = default;

  [[nodiscard]] constexpr bool
  full() const PF_NOEXCEPT {
    return m_free_head == nullptr;
  }

  [[nodiscard]] constexpr bool
  is_null() const PF_NOEXCEPT {
    return m_data == nullptr;
  }

  [[nodiscard]] constexpr SizeType
  alignment() const PF_NOEXCEPT {
    return m_alignment;
  }

  [[nodiscard]] constexpr SizeType
  stride() const PF_NOEXCEPT {
    return m_stride;
  }

  [[nodiscard]] constexpr SizeType
  block_size() const PF_NOEXCEPT {
    return m_user_block_size;
  }

  [[nodiscard]] constexpr SizeType
  block_capacity() const PF_NOEXCEPT {
    return m_block_capacity;
  }

  [[nodiscard]] constexpr SizeType
  capacity() const PF_NOEXCEPT {
    return block_capacity() * stride();
  }

  /**
   * @brief Allocates a slot in the pool, returning the pointer to the allocated
   * memory, the buffer size is the requested block size
   *
   *
   */
  [[nodiscard]] Buffer
  alloc() PF_NOEXCEPT {
    if (m_free_head == nullptr) {
      return Buffer{.data = nullptr, .size = 0};
    }
    Link* old_head = std::exchange(m_free_head, m_free_head->next);
    std::destroy_at(old_head);
    return Buffer{.data = pointer_cast<std::byte*>(old_head), .size = m_user_block_size};
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
    m_free_head = std::construct_at(pointer_cast<Link*>(ptr), Link{.next = m_free_head});
  }

private:
  struct Link {
    Link* next{nullptr};
  };

  Link* m_free_head{nullptr};
  std::byte* m_data{nullptr};
  SizeType m_block_capacity{0};
  SizeType m_alignment{0};
  SizeType m_stride{0};
  SizeType m_user_block_size{0};

  constexpr void
  initialise_free_list() PF_NOEXCEPT {
    Link* head{nullptr};
    for (std::size_t i = block_capacity(); i > 0; i--) {
      head = std::construct_at(pointer_cast<Link*>(&m_data[(i - 1) * stride()]),
                               Link{.next = head});
    }
    m_free_head = head;
  }
};

}
