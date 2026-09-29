module;

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstring>
#include <memory>
#include <limits>
#include <utility>

#include <PixelForge/core/macros.hpp>

export module PixelForge.mem:pool_arena;

import PixelForge.core;
import :align;

namespace pf::mem {

class ConcurrentPoolArena {
  public:
    using size_type  = std::size_t;
    constexpr ConcurrentPoolArena() PF_NOEXCEPT = default;

    struct Slot {
      Buffer buffer;
      size_type blockIdx{}; 
      [[nodiscard]] constexpr bool isNull() const PF_NOEXCEPT {
        return buffer.isNull();
      }
    };

    struct CreateParams {
      Buffer& buffer;
      size_type block_size{};
      size_type block_count{};
      size_type alignment{0};
    };

    ConcurrentPoolArena(const CreateParams& params)
      : m_data(params.buffer.data),
      m_capMask(params.block_count - 1), 
      m_stride(align(params.block_size, params.alignment)),
      m_alignment(params.alignment),
      m_userBlockSize(params.block_size),
      m_freeHead(0) PF_NOEXCEPT
    {
      PF_REQUIRE(std::has_single_bit(params.block_size));
      PF_REQUIRE(pointer_cast<std::uintptr_t>(params.buffer.data) % params.alignment == 0);
      PF_REQUIRE(params.buffer.size == params.block_count * m_stride);
      initialiseFreeList_();
    }

    ConcurrentPoolArena(const ConcurrentPoolArena&) = delete;
    ConcurrentPoolArena& operator=(const ConcurrentPoolArena&) = delete;

    ConcurrentPoolArena(ConcurrentPoolArena&& other) PF_NOEXCEPT :
      m_data(std::exchange(other.m_data, nullptr)),
      m_capMask(std::exchange(other.m_capMask, 0)),
      m_stride(other.m_stride), 
      m_alignment(other.m_alignment),
      m_userBlockSize(other.m_userBlockSize),
      m_freeHead(other.m_freeHead.exchange(null, std::memory_order::seq_cst)) {}

    ConcurrentPoolArena& operator=(ConcurrentPoolArena&& other) PF_NOEXCEPT {
      if (this != &other) {
        PF_REQUIRE(isNull());
        m_data = other.m_data;
        m_capMask = std::exchange(other.m_capMask, 0);
        m_stride = other.m_stride;
        m_alignment = other.m_alignment;
        m_userBlockSize = other.m_userBlockSize;
        m_freeHead = other.m_freeHead.exchange(null, std::memory_order::seq_cst);
      }
      return *this;
    }

    ~ConcurrentPoolArena() PF_NOEXCEPT {
      PF_REQUIRE(m_freeHead == null, "intended not to free everything?");
    }

    [[nodiscard]] constexpr size_type capacity() const PF_NOEXCEPT {
      return m_capMask + 1;
    }

    [[nodiscard]] constexpr bool full(const std::memory_order& order = std::memory_order::acquire) const PF_NOEXCEPT {
      return m_freeHead.load(order) == null;
    }

    [[nodiscard]] constexpr bool isNull() const PF_NOEXCEPT {
      return m_data == nullptr;
    }

    [[nodiscard]] constexpr size_type alignment() const PF_NOEXCEPT {
      return m_alignment;
    } 

    [[nodiscard]] constexpr size_type stride() const PF_NOEXCEPT {
      return m_stride;
    }

    [[nodiscard]] constexpr size_type blockSize() const PF_NOEXCEPT {
      return m_userBlockSize;
    }

    [[nodiscard]] constexpr const std::byte* resolve(size_type blockIdx) const PF_NOEXCEPT {
      PF_REQUIRE(blockIdx < capacity());
      return &m_data[blockIdx * stride()];
    }

    [[nodiscard]] constexpr std::byte* resolve(size_type blockIdx) PF_NOEXCEPT {
      PF_REQUIRE(blockIdx < capacity());
      return &m_data[blockIdx * stride()];
    }

    [[nodiscard]] constexpr const std::byte* resolve(const Slot& slot) const PF_NOEXCEPT {
      return resolve(slot.blockIdx);
    }

    [[nodiscard]] constexpr std::byte* resolve(const Slot& slot) PF_NOEXCEPT {
      return resolve(slot.blockIdx);
    }

    /**
     * @brief Allocates a slot in the pool, returning the pointer to the allocated
     * memory, the buffer size is the stride
     *
     *
    */
    [[nodiscard]] Slot alloc() PF_NOEXCEPT {
      size_type head = m_freeHead.load(std::memory_order_acquire);
      
      for (;;) {
        if (head == null) { return Slot{ .buffer = Buffer::null() }; }
        
        const size_type headBlockIdx = head & m_capMask;
        const size_type nextBlockIdx = getNextBlockIdx_(headBlockIdx);
        const size_type generation = generationOf_(head);

        if (m_freeHead.compare_exchange_weak(
              head,
              tagIdx_(generation, nextBlockIdx),
              std::memory_order_acq_rel)
            ) {
          std::destroy_at(pointer_cast<size_type*>(&m_data[headBlockIdx * stride()]));
          return Slot {
            .buffer {
              .data = resolve(headBlockIdx),
              .size = m_userBlockSize
            },
            .blockIdx = headBlockIdx
          };
        }
        
      }
    }

    void dealloc(size_type blockIdx) PF_NOEXCEPT {
      size_type head = m_freeHead.load(std::memory_order_acquire);
      size_type* const ptr = pointer_cast<size_type*>(resolve(blockIdx));
      std::construct_at(ptr, indexOf_(head));
      for (;;) {
        const size_type generation = generationOf_(head);
        if (m_freeHead.compare_exchange_weak(head,
              tagIdx_(generation, blockIdx),
              std::memory_order_acquire
              )) {
          return;
        }
        *ptr = indexOf_(head);
      }
    }

    void dealloc(std::byte* ptr) PF_NOEXCEPT {
      PF_REQUIRE_ASSUME((pointer_cast<std::uintptr_t>(ptr) - pointer_cast<std::uintptr_t>(m_data)) % m_stride == 0);
      const size_type blockIdx =
        (pointer_cast<std::uintptr_t>(ptr) - pointer_cast<std::uintptr_t>(m_data)) / m_stride;

      dealloc(blockIdx);
    }


  private:
    static constexpr size_type null = std::numeric_limits<size_type>::max();
    
    std::byte* m_data{nullptr};
    size_type m_capMask{0}; // in units of the stride
    size_type m_stride{0};
    size_type m_alignment{0}; 
    size_type m_userBlockSize{0};
    
    PF_CACHE_LINE_ALIGN_VAR
    std::atomic<size_type> m_freeHead{null};

    constexpr void initialiseFreeList_() PF_NOEXCEPT {
      for (size_type i = 0; i < capacity(); i++) {
        std::construct_at(pointer_cast<size_type*>(&m_data[i * stride()]), i);
      }
    }

    constexpr size_type getNextBlockIdx_(size_type blockIdx) const PF_NOEXCEPT {
      size_type nextIdx{0};
      std::memcpy(&m_data[blockIdx * stride()], &nextIdx, sizeof(size_type));
      return nextIdx;
    }

    [[nodiscard]] constexpr size_type indexOf_(size_type tagged) const PF_NOEXCEPT {
        return tagged & m_capMask;
    }

    [[nodiscard]] constexpr size_type generationOf_(size_type tagged) const PF_NOEXCEPT {
        return (tagged & ~m_capMask);
    }

    [[nodiscard]] constexpr size_type tagIdx_(size_type generation,
                   size_type index) const PF_NOEXCEPT {
        return (generation)
             | (index & m_capMask);
    }

    [[nodiscard]] constexpr size_type bumpGeneration_(size_type tagged) const PF_NOEXCEPT {
        return tagged + capacity();
    }

};

}
