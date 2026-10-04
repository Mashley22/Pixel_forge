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
    size_type block_idx{};
    [[nodiscard]] constexpr bool
    is_null() const PF_NOEXCEPT {
      return buffer.is_null();
    }
  };

  struct CreateParams {
    Buffer buffer;
    size_type block_size{};
    size_type block_count{};
    size_type alignment{0};
  };

  ConcurrentPoolArena(const CreateParams& params)
    : m_free_head(0), m_data(params.buffer.data), m_cap_mask(params.block_count - 1),
      m_alignment(std::max(params.alignment, alignof(size_type))),
      m_stride(std::max(align(params.block_size, alignment()), sizeof(size_type))),
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

  [[nodiscard]] constexpr size_type
  block_capacity() const PF_NOEXCEPT {
    PF_REQUIRE(!is_null(), "a null arena has no capacity");
    return m_cap_mask + 1;
  }

  [[nodiscard]] constexpr size_type
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

  [[nodiscard]] constexpr size_type
  alignment() const PF_NOEXCEPT {
    return m_alignment;
  }

  [[nodiscard]] constexpr size_type
  stride() const PF_NOEXCEPT {
    return m_stride;
  }

  [[nodiscard]] constexpr size_type
  block_size() const PF_NOEXCEPT {
    return m_user_block_size;
  }

  [[nodiscard]] constexpr const std::byte*
  resolve(size_type block_idx) const PF_NOEXCEPT {
    PF_REQUIRE(block_idx < block_capacity());
    return &m_data[block_idx * stride()];
  }

  [[nodiscard]] constexpr std::byte*
  resolve(size_type block_idx) PF_NOEXCEPT {
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
  allocate() PF_NOEXCEPT {
    size_type head = m_free_head.load(std::memory_order_acquire);

    for (;;) {
      if (head == null) {
        return Slot{.buffer = Buffer::null()};
      }

      const size_type head_block_idx = index_of(head);
      const size_type next_block_idx = get_next_block_idx(head_block_idx);
      const size_type generation = generation_of(head);
      const size_type next_head =
          next_block_idx == null ? null : tag_idx(generation, next_block_idx);

      if (m_free_head.compare_exchange_weak(head, next_head, std::memory_order_acq_rel)) {
        std::destroy_at(pointer_cast<size_type*>(&m_data[head_block_idx * stride()]));
        return Slot{.buffer{.data = resolve(head_block_idx), .size = m_user_block_size},
                    .block_idx = head_block_idx};
      }
    }
  }

  [[nodiscard]] Buffer
  allocate(size_type size, size_type alignment = alignof(std::max_align_t)) PF_NOEXCEPT {
    PF_REQUIRE(size <= block_size());
    PF_REQUIRE(alignment <= m_alignment);
    return allocate().buffer;
  }

  void
  deallocate(size_type block_idx) PF_NOEXCEPT {
    size_type head = m_free_head.load(std::memory_order_acquire);
    size_type* const ptr = pointer_cast<size_type*>(resolve(block_idx));
    for (;;) {
      std::construct_at(ptr, head == null ? null : index_of(head));
      const size_type generation = head == null ? 0 : generation_of(head);
      if (m_free_head.compare_exchange_weak(
              head,
              tag_idx(bump_generation(generation), block_idx),
              std::memory_order_acq_rel)) {
        return;
      }
    }
  }

  void
  deallocate(const Buffer& buffer) PF_NOEXCEPT {
    PF_REQUIRE(buffer.size <= block_size());
    deallocate(buffer.data);
  }

  void
  deallocate(std::byte* ptr) PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(
        (pointer_cast<std::uintptr_t>(ptr) - pointer_cast<std::uintptr_t>(m_data)) %
            m_stride ==
        0);
    const size_type block_idx =
        (pointer_cast<std::uintptr_t>(ptr) - pointer_cast<std::uintptr_t>(m_data)) /
        m_stride;

    deallocate(block_idx);
  }

private:
  static constexpr size_type null = std::numeric_limits<size_type>::max();

  PF_CACHE_LINE_ALIGN_VAR
  std::atomic<size_type> m_free_head{null};

  std::byte* m_data{nullptr};
  size_type m_cap_mask{0}; // in units of the stride
  size_type m_alignment{0};
  size_type m_stride{0};
  size_type m_user_block_size{0};

  constexpr void
  initialise_free_list() PF_NOEXCEPT {
    for (size_type i = 0; i < block_capacity(); i++) {
      const size_type next_idx = i + 1 == block_capacity() ? null : i + 1;
      std::construct_at(pointer_cast<size_type*>(&m_data[i * stride()]), next_idx);
    }
  }

  constexpr size_type
  get_next_block_idx(size_type block_idx) const PF_NOEXCEPT {
    size_type next_idx{0};
    std::memcpy(&next_idx, &m_data[block_idx * stride()], sizeof(size_type));
    return next_idx;
  }

  [[nodiscard]] constexpr size_type
  index_of(size_type tagged) const PF_NOEXCEPT {
    return tagged & m_cap_mask;
  }

  [[nodiscard]] constexpr size_type
  generation_of(size_type tagged) const PF_NOEXCEPT {
    return (tagged & ~m_cap_mask);
  }

  [[nodiscard]] constexpr size_type
  tag_idx(size_type generation, size_type index) const PF_NOEXCEPT {
    return (generation) | (index & m_cap_mask);
  }

  [[nodiscard]] constexpr size_type
  bump_generation(size_type generation) const PF_NOEXCEPT {
    return generation + block_capacity();
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
    : m_data(params.buffer.data), m_block_capacity(params.block_count),
      m_alignment(std::max(params.alignment, alignof(void*))),
      m_stride(std::max(align(params.block_size, alignment()), sizeof(size_type))),
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

  [[nodiscard]] constexpr size_type
  alignment() const PF_NOEXCEPT {
    return m_alignment;
  }

  [[nodiscard]] constexpr size_type
  stride() const PF_NOEXCEPT {
    return m_stride;
  }

  [[nodiscard]] constexpr size_type
  block_size() const PF_NOEXCEPT {
    return m_user_block_size;
  }

  [[nodiscard]] constexpr size_type
  block_capacity() const PF_NOEXCEPT {
    return m_block_capacity;
  }

  [[nodiscard]] constexpr size_type
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
  allocate() PF_NOEXCEPT {
    if (m_free_head == nullptr) {
      return Buffer{.data = nullptr, .size = 0};
    }
    Link* old_head = std::exchange(m_free_head, m_free_head->next);
    std::destroy_at(old_head);
    return Buffer{.data = pointer_cast<std::byte*>(old_head), .size = m_user_block_size};
  }

  [[nodiscard]] Buffer
  allocate(std::size_t bytes,
           std::size_t alignment = alignof(std::max_align_t)) PF_NOEXCEPT {
    PF_REQUIRE(bytes <= block_size());
    PF_REQUIRE(alignment <= m_alignment);
    return allocate();
  }

  void
  deallocate(const Buffer& buffer) PF_NOEXCEPT {
    PF_REQUIRE(buffer.size <= block_size());
    deallocate(buffer.data);
  }

  void
  deallocate(std::byte* ptr) PF_NOEXCEPT {
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
  size_type m_block_capacity{0};
  size_type m_alignment{0};
  size_type m_stride{0};
  size_type m_user_block_size{0};

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
