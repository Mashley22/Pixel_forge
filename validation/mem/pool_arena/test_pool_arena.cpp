#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <numeric>
#include <thread>
#include <vector>

#include <PixelForgeValidationHelpers/helpers.hpp>
#include <catch2/catch_test_macros.hpp>

import PixelForge.core;
import PixelForge.mem;

namespace pf::mem {
namespace {

constexpr std::size_t block_size = 64;
constexpr std::size_t blockCount = 8;
constexpr std::size_t alignment = 64;

/**@brief Mirrors the stride the arena computes for the given block geometry */
[[nodiscard]] constexpr std::size_t
strideOf(std::size_t blkSize, std::size_t align) {
  return ((blkSize + align - 1) / align) * align;
}

/**
 * @brief Owns the backing storage and keeps the CreateParams alive, as the
 * arenas hold a reference to the Buffer for their whole life.
 *
 * @tparam ArenaT the arena the params belong to, i.e ConcurrentPoolArena or
 *         PoolArena
 */
template <std::size_t StorageSize,
          std::size_t BlockSize = block_size,
          std::size_t BlockCount = StorageSize / strideOf(BlockSize, alignment),
          std::size_t Alignment = alignment,
          typename ArenaT = ConcurrentPoolArena>
class PoolStorage {
public:
  using CreateParams = typename ArenaT::CreateParams;

  static_assert(Alignment != 0, "alignment must be non zero");
  static_assert(StorageSize >= BlockCount * strideOf(BlockSize, Alignment),
                "storage is too small for the requested blocks");

  PoolStorage()
    : m_buffer(Buffer::from(static_cast<std::byte*>(m_storage.data()), m_storage.size())),
      m_params{.buffer = m_buffer,
               .block_size = BlockSize,
               .block_count = BlockCount,
               .alignment = Alignment} {}

  [[nodiscard]] CreateParams&
  params() {
    return m_params;
  }

  [[nodiscard]] std::byte*
  data() {
    return m_storage.data();
  }

private:
  alignas(std::hardware_destructive_interference_size)
      std::array<std::byte, StorageSize> m_storage{};
  Buffer m_buffer{Buffer::null()};
  CreateParams m_params;
};

/**@brief Fills a whole block with a tag derived from the block index */
void
tagBlock(std::byte* block, std::size_t block_idx, std::size_t size) {
  std::fill_n(block, size, static_cast<std::byte>(block_idx + 1));
}

[[nodiscard]] bool
isTagged(const ConcurrentPoolArena::Slot& slot) {
  return std::all_of(slot.buffer.data,
                     slot.buffer.data + slot.buffer.size,
                     [idx = slot.block_idx](std::byte byte) {
                       return byte == static_cast<std::byte>(idx + 1);
                     });
}

/**@brief Allocates the whole pool, returning the block indices in hand out order */
[[nodiscard]] std::vector<ConcurrentPoolArena::Slot>
drain(ConcurrentPoolArena& arena) {
  std::vector<ConcurrentPoolArena::Slot> slots;
  for (std::size_t i = 0; i < arena.block_capacity(); i++) {
    const auto slot = arena.allocate();
    REQUIRE_FALSE(slot.is_null());
    slots.push_back(slot);
  }
  return slots;
}

[[nodiscard]] std::vector<std::size_t>
sortedIndices(const std::vector<ConcurrentPoolArena::Slot>& slots) {
  std::vector<std::size_t> indices;
  indices.reserve(slots.size());
  for (const auto& slot : slots) {
    indices.push_back(slot.block_idx);
  }
  std::sort(indices.begin(), indices.end());
  return indices;
}

/**@brief Allocates `count` blocks from a single threaded arena */
[[nodiscard]] std::vector<std::byte*>
drainSt(PoolArena& arena, std::size_t count) {
  std::vector<std::byte*> blocks;
  blocks.reserve(count);
  for (std::size_t i = 0; i < count; i++) {
    const Buffer block = arena.allocate();
    REQUIRE_FALSE(block.is_null());
    blocks.push_back(block.data);
  }
  return blocks;
}

/**@brief Allocates the whole pool of a single threaded arena */
[[nodiscard]] std::vector<std::byte*>
drainSt(PoolArena& arena) {
  return drainSt(arena, arena.block_capacity());
}

/**@brief The block indices of an arbitrary hand out, sorted ascending */
[[nodiscard]] std::vector<std::size_t>
sortedIndicesSt(const std::vector<std::byte*>& blocks,
                const PoolArena& arena,
                std::byte* base) {
  std::vector<std::size_t> indices;
  indices.reserve(blocks.size());
  for (auto* block : blocks) {
    indices.push_back(static_cast<std::size_t>(block - base) / arena.stride());
  }
  std::sort(indices.begin(), indices.end());
  return indices;
}

/**@brief No block is handed out twice */
[[nodiscard]] bool
allDistinct(const std::vector<std::byte*>& blocks) {
  std::vector<std::byte*> copy = blocks;
  std::sort(copy.begin(), copy.end());
  return std::adjacent_find(copy.begin(), copy.end()) == copy.end();
}

} // namespace

PF_TEST_CASE("pool arena exposes the geometry it was built with", "[mem][poolArena]") {
  PoolStorage<blockCount * block_size> storage;
  ConcurrentPoolArena arena{storage.params()};

  REQUIRE_FALSE(arena.is_null());
  REQUIRE(arena.block_capacity() == blockCount);
  REQUIRE(arena.block_size() == block_size);
  REQUIRE(arena.stride() == block_size);
  REQUIRE(arena.alignment() == alignment);
  REQUIRE_FALSE(arena.full());
  REQUIRE(arena.resolve(0) == storage.data());
  REQUIRE(arena.resolve(blockCount - 1) ==
          storage.data() + (blockCount - 1) * block_size);
}

PF_TEST_CASE("alloc hands out distinct, tagged, in range blocks", "[mem][poolArena]") {
  PoolStorage<blockCount * block_size> storage;
  ConcurrentPoolArena arena{storage.params()};

  const auto slots = drain(arena);

  for (std::size_t i = 0; i < blockCount; i++) {
    const auto& slot = slots[i];
    REQUIRE_FALSE(slot.buffer.is_null());
    REQUIRE(slot.buffer.size == block_size);
    REQUIRE(slot.buffer.data == arena.resolve(slot.block_idx));
    REQUIRE(slot.buffer.data >= storage.data());
    REQUIRE(slot.buffer.data + block_size <= storage.data() + block_size * blockCount);
  }

  for (std::size_t i = 0; i < blockCount; i++) {
    tagBlock(slots[i].buffer.data, slots[i].block_idx, block_size);
  }

  for (std::size_t i = 0; i < blockCount; i++) {
    for (std::size_t j = i + 1; j < blockCount; j++) {
      REQUIRE(slots[i].block_idx != slots[j].block_idx);
      REQUIRE(slots[i].buffer.data != slots[j].buffer.data);
    }
    // no block may have been clobbered by a later allocation
    REQUIRE(isTagged(slots[i]));
  }

  for (const auto& slot : slots) {
    arena.deallocate(slot.block_idx);
  }
}

PF_TEST_CASE("alloc yields a null slot once the pool is exhausted", "[mem][poolArena]") {
  PoolStorage<blockCount * block_size> storage;
  ConcurrentPoolArena arena{storage.params()};

  const auto slots = drain(arena);
  REQUIRE(arena.full());

  const auto exhausted = arena.allocate();
  REQUIRE(exhausted.is_null());
  REQUIRE(exhausted.buffer.is_null());
  REQUIRE(arena.full());

  // every block must have been handed out exactly once
  const auto indices = sortedIndices(slots);
  REQUIRE(std::adjacent_find(indices.begin(), indices.end()) == indices.end());
  std::vector<std::size_t> expected(blockCount);
  std::iota(expected.begin(), expected.end(), std::size_t{0});
  REQUIRE(indices == expected);

  for (const auto& slot : slots) {
    arena.deallocate(slot.block_idx);
  }
}

PF_TEST_CASE("dealloc returns the block and the pool reports free capacity",
             "[mem][poolArena]") {
  PoolStorage<blockCount * block_size> storage;
  ConcurrentPoolArena arena{storage.params()};

  const auto slots = drain(arena);
  REQUIRE(arena.full());

  constexpr std::size_t freed = 3;
  arena.deallocate(slots[freed].block_idx);
  REQUIRE_FALSE(arena.full());

  const auto reused = arena.allocate();
  REQUIRE_FALSE(reused.is_null());
  REQUIRE(reused.block_idx == slots[freed].block_idx);
  REQUIRE(reused.buffer.data == slots[freed].buffer.data);
  REQUIRE(arena.full());

  for (const auto& slot : slots) {
    if (slot.block_idx != freed) {
      arena.deallocate(slot.block_idx);
    }
  }
  REQUIRE_FALSE(arena.full());
}

PF_TEST_CASE("free blocks are handed back out last in, first out", "[mem][poolArena]") {
  PoolStorage<blockCount * block_size> storage;
  ConcurrentPoolArena arena{storage.params()};

  const auto slots = drain(arena);

  std::vector<std::size_t> freed;
  for (std::size_t i = slots.size(); i > 0; i--) {
    arena.deallocate(slots[i - 1].block_idx);
    freed.push_back(slots[i - 1].block_idx);
  }
  REQUIRE_FALSE(arena.full());

  // the free list is a stack, so the block freed last comes out first
  for (auto it = freed.rbegin(); it != freed.rend(); ++it) {
    const auto slot = arena.allocate();
    REQUIRE_FALSE(slot.is_null());
    REQUIRE(slot.block_idx == *it);
  }
  REQUIRE(arena.full());
}

PF_TEST_CASE("dealloc by pointer matches dealloc by index", "[mem][poolArena]") {
  PoolStorage<blockCount * block_size> storage;
  ConcurrentPoolArena arena{storage.params()};

  const auto slots = drain(arena);

  arena.deallocate(arena.resolve(slots[2].block_idx));
  arena.deallocate(slots[4].buffer.data);
  REQUIRE_FALSE(arena.full());

  const auto first = arena.allocate();
  const auto second = arena.allocate();
  REQUIRE(first.block_idx == slots[4].block_idx);
  REQUIRE(second.block_idx == slots[2].block_idx);
  REQUIRE(arena.full());

  for (const auto& slot : slots) {
    if (slot.block_idx != 2 && slot.block_idx != 4) {
      arena.deallocate(slot.block_idx);
    }
  }
  REQUIRE_FALSE(arena.full());
}

PF_TEST_CASE("churning the pool never loses or duplicates a block", "[mem][poolArena]") {
  PoolStorage<blockCount * block_size> storage;
  ConcurrentPoolArena arena{storage.params()};

  for (std::size_t round = 0; round < 32; round++) {
    const auto slots = drain(arena);
    REQUIRE(arena.full());

    const auto indices = sortedIndices(slots);
    REQUIRE(std::adjacent_find(indices.begin(), indices.end()) == indices.end());
    REQUIRE(indices.front() == 0);
    REQUIRE(indices.back() == blockCount - 1);

    for (const auto idx : indices) {
      arena.deallocate(idx);
    }
    REQUIRE_FALSE(arena.full());
  }
}

PF_TEST_CASE("blocks smaller than the free list link still work", "[mem][poolArena]") {
  constexpr std::size_t tinyBlockSize = 4;
  constexpr std::size_t tinyCount = 4;
  constexpr std::size_t tinyAlignment = 8;
  PoolStorage<tinyCount * tinyAlignment, tinyBlockSize, tinyCount, tinyAlignment> storage;
  ConcurrentPoolArena arena{storage.params()};

  REQUIRE(arena.block_capacity() == tinyCount);
  REQUIRE(arena.stride() == tinyAlignment);
  REQUIRE(arena.block_size() == tinyBlockSize);

  auto slots = drain(arena);
  REQUIRE(arena.full());

  for (auto& slot : slots) {
    REQUIRE(slot.buffer.size == tinyBlockSize);
    tagBlock(slot.buffer.data, slot.block_idx, tinyBlockSize);
  }
  for (const auto& slot : slots) {
    REQUIRE(isTagged(slot));
    arena.deallocate(slot.block_idx);
  }
  REQUIRE_FALSE(arena.full());

  slots = drain(arena);
  REQUIRE(arena.full());
  for (const auto& slot : slots) {
    arena.deallocate(slot.block_idx);
  }
  REQUIRE_FALSE(arena.full());
}

PF_TEST_CASE("padded stride keeps every block aligned", "[mem][poolArena]") {
  constexpr std::size_t unalignedBlockSize = 32;
  constexpr std::size_t strictAlignment = 64;
  constexpr std::size_t count = 4;
  PoolStorage<count * strictAlignment, unalignedBlockSize, count, strictAlignment>
      storage;
  ConcurrentPoolArena arena{storage.params()};

  REQUIRE(arena.block_capacity() == count);
  REQUIRE(arena.block_size() == unalignedBlockSize);
  REQUIRE(arena.stride() == strictAlignment);
  REQUIRE(arena.alignment() == strictAlignment);

  const auto slots = drain(arena);
  for (const auto& slot : slots) {
    REQUIRE(pointer_cast<std::uintptr_t>(slot.buffer.data) % strictAlignment == 0);
    REQUIRE(slot.buffer.size == unalignedBlockSize);
    // the whole stride, not just the user block, must not overlap the next one
    REQUIRE(slot.buffer.data + arena.stride() <=
            storage.data() + count * strictAlignment);
  }
  REQUIRE(arena.full());
  for (const auto& slot : slots) {
    arena.deallocate(slot.block_idx);
  }
}

PF_TEST_CASE("a default constructed arena is null and empty", "[mem][poolArena]") {
  ConcurrentPoolArena arena;
  REQUIRE(arena.is_null());
  REQUIRE(arena.full());
  REQUIRE(arena.allocate().is_null());
}

PF_TEST_CASE("destruction does not demand a drained pool", "[mem][poolArena]") {
  // the arena does not own the blocks, so outstanding allocations are the
  // caller's business, destroying must not require or forbid any state
  for (std::size_t keep = 0; keep <= blockCount; keep++) {
    PoolStorage<blockCount * block_size> storage;
    ConcurrentPoolArena arena{storage.params()};
    for (std::size_t i = 0; i < keep; i++) {
      REQUIRE_FALSE(arena.allocate().is_null());
    }
  }
}

PF_TEST_CASE("resolve rejects indices outside the pool", "[mem][poolArena]") {
  PoolStorage<blockCount * block_size> storage;
  ConcurrentPoolArena arena{storage.params()};

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  REQUIRE_PF_REQUIRE_FAIL(arena.resolve(arena.block_capacity()));
  REQUIRE_PF_REQUIRE_FAIL(arena.resolve(std::numeric_limits<std::size_t>::max()));
#endif

  const auto slot = arena.allocate();
  REQUIRE(arena.resolve(slot) == slot.buffer.data);
  arena.deallocate(slot.block_idx);
}

PF_TEST_CASE("create params are validated", "[mem][poolArena]") {
#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  {
    // the block size must be a power of two
    alignas(alignment) std::array<std::byte, block_size * blockCount> storage{};
    Buffer buffer{Buffer::from(static_cast<std::byte*>(storage.data()), storage.size())};
    ConcurrentPoolArena::CreateParams params{.buffer = buffer,
                                             .block_size = block_size - 1,
                                             .block_count = blockCount,
                                             .alignment = alignment};
    const auto makeArena = [&params]() { return ConcurrentPoolArena{params}; };
    REQUIRE_PF_REQUIRE_FAIL(makeArena());
  }

  {
    // the buffer must be exactly block_count * stride bytes
    alignas(alignment) std::array<std::byte, block_size * blockCount> storage{};
    Buffer buffer{
        Buffer::from(static_cast<std::byte*>(storage.data()), storage.size() - 1)};
    ConcurrentPoolArena::CreateParams params{.buffer = buffer,
                                             .block_size = block_size,
                                             .block_count = blockCount,
                                             .alignment = alignment};
    const auto makeArena = [&params]() { return ConcurrentPoolArena{params}; };
    REQUIRE_PF_REQUIRE_FAIL(makeArena());
  }

  {
    // the base pointer must satisfy the requested alignment
    alignas(alignment) std::array<std::byte, block_size * blockCount> storage{};
    Buffer buffer{Buffer::from(static_cast<std::byte*>(storage.data()) + 1,
                               block_size * blockCount)};
    ConcurrentPoolArena::CreateParams params{.buffer = buffer,
                                             .block_size = block_size,
                                             .block_count = blockCount,
                                             .alignment = alignment};
    const auto makeArena = [&params]() { return ConcurrentPoolArena{params}; };
    REQUIRE_PF_REQUIRE_FAIL(makeArena());
  }
#endif
}

PF_TEST_CASE("move construction keeps the pool and nulls the source",
             "[mem][poolArena]") {
  PoolStorage<blockCount * block_size> storage;
  ConcurrentPoolArena arena{storage.params()};

  const auto held = arena.allocate();
  REQUIRE_FALSE(held.is_null());

  ConcurrentPoolArena moved{std::move(arena)};
  REQUIRE(arena.is_null());
  REQUIRE(arena.full());
  REQUIRE(arena.allocate().is_null());

  REQUIRE_FALSE(moved.is_null());
  REQUIRE(moved.block_capacity() == blockCount);
  REQUIRE(moved.block_size() == block_size);
  REQUIRE(moved.alignment() == alignment);
  // the block held before the move is still owned, not handed out again
  REQUIRE(moved.resolve(held.block_idx) == held.buffer.data);

  const auto next = moved.allocate();
  REQUIRE_FALSE(next.is_null());
  REQUIRE(next.block_idx != held.block_idx);

  moved.deallocate(next.block_idx);
  moved.deallocate(held.block_idx);
}

PF_TEST_CASE("move assignment fills a null arena and rejects a live one",
             "[mem][poolArena]") {
  PoolStorage<blockCount * block_size> storage;
  ConcurrentPoolArena arena{storage.params()};

  ConcurrentPoolArena target;
  REQUIRE(target.is_null());

  target = std::move(arena);
  REQUIRE(arena.is_null());
  REQUIRE_FALSE(target.is_null());
  REQUIRE(target.block_capacity() == blockCount);
  REQUIRE_FALSE(target.full());

  const auto slot = target.allocate();
  REQUIRE_FALSE(slot.is_null());
  target.deallocate(slot.block_idx);

  ConcurrentPoolArena live{storage.params()};
#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  REQUIRE_PF_REQUIRE_FAIL(live = std::move(target));
#endif
  live.deallocate(live.allocate().block_idx);
}

PF_TEST_CASE("concurrent alloc dealloc churn never double hands a block out",
             "[mem][poolArena]") {
  constexpr std::size_t threadCount = 8;
  constexpr std::size_t iterations = 2000;

  PoolStorage<blockCount * block_size> storage;
  ConcurrentPoolArena arena{storage.params()};

  std::array<std::atomic<std::uint32_t>, blockCount> claims{};
  std::atomic<std::size_t> doubleClaims{0};
  std::atomic<std::size_t> nullAllocs{0};
  std::atomic<std::size_t> successfulAllocs{0};

  const auto worker = [&arena, &claims, &doubleClaims, &nullAllocs, &successfulAllocs]() {
    for (std::size_t i = 0; i < iterations; i++) {
      const auto slot = arena.allocate();
      if (slot.is_null()) {
        nullAllocs.fetch_add(1, std::memory_order::relaxed);
        continue;
      }
      successfulAllocs.fetch_add(1, std::memory_order::relaxed);
      if (claims[slot.block_idx].fetch_add(1, std::memory_order::acq_rel) != 0) {
        doubleClaims.fetch_add(1, std::memory_order::relaxed);
      }
      claims[slot.block_idx].fetch_sub(1, std::memory_order::acq_rel);
      arena.deallocate(slot.buffer.data);
    }
  };

  std::vector<std::thread> workers;
  workers.reserve(threadCount);
  for (std::size_t i = 0; i < threadCount; i++) {
    workers.emplace_back(worker);
  }
  for (auto& thread : workers) {
    thread.join();
  }

  REQUIRE(doubleClaims.load(std::memory_order::relaxed) == 0);
  REQUIRE(successfulAllocs.load(std::memory_order::relaxed) +
              nullAllocs.load(std::memory_order::relaxed) ==
          threadCount * iterations);

  // every block must be back in the pool once the churn is over
  REQUIRE_FALSE(arena.full());
  const auto slots = drain(arena);
  REQUIRE(arena.full());
  const auto indices = sortedIndices(slots);
  REQUIRE(std::adjacent_find(indices.begin(), indices.end()) == indices.end());
  for (const auto idx : indices) {
    arena.deallocate(idx);
  }
}

PF_TEST_CASE("concurrent hand out gives every block to exactly one thread",
             "[mem][poolArena]") {
  constexpr std::size_t threadCount = blockCount;

  PoolStorage<blockCount * block_size> storage;
  ConcurrentPoolArena arena{storage.params()};

  std::array<std::atomic<bool>, blockCount> claimed{};
  std::atomic<std::size_t> winners{0};
  std::vector<std::vector<std::size_t>> taken(threadCount);
  std::atomic<std::size_t> retries{0};

  const auto worker = [&arena, &claimed, &winners, &taken, &retries](std::size_t id) {
    while (arena.full()) {
      retries.fetch_add(1, std::memory_order::relaxed);
      std::this_thread::yield();
    }
    const auto slot = arena.allocate();
    if (slot.is_null()) {
      return;
    }
    if (!claimed[slot.block_idx].exchange(true, std::memory_order::acq_rel)) {
      winners.fetch_add(1, std::memory_order::relaxed);
      taken[id].push_back(slot.block_idx);
    }
  };

  std::vector<std::thread> workers;
  workers.reserve(threadCount);
  for (std::size_t i = 0; i < threadCount; i++) {
    workers.emplace_back(worker, i);
  }
  for (auto& thread : workers) {
    thread.join();
  }

  REQUIRE(winners.load(std::memory_order::relaxed) == blockCount);

  std::vector<std::size_t> all;
  for (const auto& threadTaken : taken) {
    all.insert(all.end(), threadTaken.begin(), threadTaken.end());
  }
  REQUIRE(all.size() == blockCount);
  std::sort(all.begin(), all.end());
  REQUIRE(std::adjacent_find(all.begin(), all.end()) == all.end());
  std::vector<std::size_t> expected(blockCount);
  std::iota(expected.begin(), expected.end(), std::size_t{0});
  REQUIRE(all == expected);

  for (const auto idx : all) {
    arena.deallocate(idx);
  }
  REQUIRE_FALSE(arena.full());
}

//----------------------------------------------------------------------------
// pf::mem::PoolArena, the single threaded pool arena
//----------------------------------------------------------------------------

PF_TEST_CASE("single threaded arena exposes the geometry it was built with",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * block_size, block_size, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  REQUIRE_FALSE(arena.is_null());
  REQUIRE_FALSE(arena.full());
  REQUIRE(arena.block_capacity() == blockCount);
  REQUIRE(arena.block_size() == block_size);
  REQUIRE(arena.stride() == block_size);
  REQUIRE(arena.alignment() == alignment);
  REQUIRE(arena.capacity() == blockCount * block_size);
}

PF_TEST_CASE("single threaded alloc hands out distinct, in range blocks",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * block_size, block_size, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  const auto blocks = drainSt(arena);
  REQUIRE(blocks.size() == blockCount);
  REQUIRE(allDistinct(blocks));

  for (auto* block : blocks) {
    REQUIRE(block >= storage.data());
    REQUIRE(block + block_size <= storage.data() + blockCount * block_size);
    // a block must start on a stride boundary inside the pool
    REQUIRE(static_cast<std::size_t>(block - storage.data()) % arena.stride() == 0);
  }

  // every block of the pool must have been handed out, exactly once
  const auto indices = sortedIndicesSt(blocks, arena, storage.data());
  std::vector<std::size_t> expected(blockCount);
  std::iota(expected.begin(), expected.end(), std::size_t{0});
  REQUIRE(indices == expected);

  for (auto* block : blocks) {
    arena.deallocate(block);
  }
}

PF_TEST_CASE(
    "single threaded alloc reports the requested block size and keeps payloads intact",
    "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * block_size, block_size, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  // hold a single block back, tag everything else, then free it
  const Buffer held = arena.allocate();
  REQUIRE_FALSE(held.is_null());
  REQUIRE(held.size == arena.block_size());

  std::vector<std::byte*> tagged;
  while (!arena.full()) {
    const Buffer block = arena.allocate();
    REQUIRE_FALSE(block.is_null());
    REQUIRE(block.size == block_size);
    tagBlock(block.data, tagged.size(), block.size);
    tagged.push_back(block.data);
  }
  REQUIRE(tagged.size() == blockCount - 1);

  arena.deallocate(held.data);
  const Buffer reused = arena.allocate();
  REQUIRE(reused.data == held.data);

  // a later allocation must never clobber an earlier block
  for (std::size_t i = 0; i < tagged.size(); i++) {
    REQUIRE(std::all_of(
        tagged[i],
        tagged[i] + block_size,
        [tag = static_cast<std::byte>(i + 1)](std::byte byte) { return byte == tag; }));
  }

  for (auto* block : tagged) {
    arena.deallocate(block);
  }
  arena.deallocate(reused.data);
  REQUIRE_FALSE(arena.full());
}

PF_TEST_CASE("single threaded alloc yields a null buffer once exhausted",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * block_size, block_size, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  const auto blocks = drainSt(arena);
  REQUIRE(arena.full());

  const Buffer exhausted = arena.allocate();
  REQUIRE(exhausted.is_null());
  REQUIRE(exhausted.data == nullptr);
  REQUIRE(exhausted.size == 0);
  REQUIRE(arena.full());

  // an exhausted pool must stay exhausted and must not lose any block
  for (std::size_t i = 0; i < 3; i++) {
    REQUIRE(arena.allocate().is_null());
  }
  REQUIRE(arena.full());

  for (auto* block : blocks) {
    arena.deallocate(block);
  }
  REQUIRE_FALSE(arena.full());
  REQUIRE(drainSt(arena).size() == blockCount);
}

PF_TEST_CASE(
    "single threaded dealloc returns the block and the pool reports free capacity",
    "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * block_size, block_size, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  const auto blocks = drainSt(arena);
  REQUIRE(arena.full());

  constexpr std::size_t freed = 3;
  arena.deallocate(blocks[freed]);
  REQUIRE_FALSE(arena.full());

  const Buffer reused = arena.allocate();
  REQUIRE_FALSE(reused.is_null());
  REQUIRE(reused.data == blocks[freed]);
  REQUIRE(reused.size == block_size);
  REQUIRE(arena.full());

  for (std::size_t i = 0; i < blocks.size(); i++) {
    if (i != freed) {
      arena.deallocate(blocks[i]);
    }
  }
  // the block handed back out above is still held
  arena.deallocate(reused.data);
  REQUIRE_FALSE(arena.full());
  REQUIRE(drainSt(arena).size() == blockCount);
}

PF_TEST_CASE("single threaded free blocks are handed back out last in, first out",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * block_size, block_size, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  const auto blocks = drainSt(arena);

  std::vector<std::byte*> freed;
  for (std::size_t i = blocks.size(); i > 0; i--) {
    arena.deallocate(blocks[i - 1]);
    freed.push_back(blocks[i - 1]);
  }
  REQUIRE_FALSE(arena.full());

  // the free list is a stack, so the block freed last comes out first
  for (auto it = freed.rbegin(); it != freed.rend(); ++it) {
    const Buffer block = arena.allocate();
    REQUIRE_FALSE(block.is_null());
    REQUIRE(block.data == *it);
  }
  REQUIRE(arena.full());
  REQUIRE(arena.allocate().is_null());
}

PF_TEST_CASE("single threaded churn never loses or duplicates a block",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * block_size, block_size, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  for (std::size_t round = 0; round < 32; round++) {
    const auto blocks = drainSt(arena);
    REQUIRE(arena.full());
    REQUIRE(allDistinct(blocks));

    const auto indices = sortedIndicesSt(blocks, arena, storage.data());
    REQUIRE(std::adjacent_find(indices.begin(), indices.end()) == indices.end());
    REQUIRE(indices.front() == 0);
    REQUIRE(indices.back() == blockCount - 1);

    for (auto* block : blocks) {
      arena.deallocate(block);
    }
    REQUIRE_FALSE(arena.full());
  }
}

PF_TEST_CASE("single threaded interleaved alloc and dealloc keeps the pool leak free",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * block_size, block_size, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  std::vector<std::byte*> live;
  std::uint32_t rng = 0x12345678;
  const auto nextRandom = [&rng]() {
    rng = (rng * 1103515245U) + 12345U;
    return rng >> 16U;
  };

  for (std::size_t round = 0; round < 512; round++) {
    const bool grow = live.empty() || (nextRandom() % 3U) != 0U;
    if (grow) {
      const Buffer block = arena.allocate();
      if (block.is_null()) {
        REQUIRE(live.size() == blockCount);
        REQUIRE(arena.full());
      } else {
        REQUIRE(block.size == block_size);
        live.push_back(block.data);
        REQUIRE(live.size() <= blockCount);
      }
    } else {
      const auto victim = nextRandom() % live.size();
      arena.deallocate(live[victim]);
      live.erase(live.begin() + static_cast<std::ptrdiff_t>(victim));
      REQUIRE_FALSE(arena.full());
    }

    REQUIRE(allDistinct(live));
    REQUIRE(arena.full() == (live.size() == blockCount));
  }

  for (auto* block : live) {
    arena.deallocate(block);
  }
  REQUIRE_FALSE(arena.full());
  REQUIRE(drainSt(arena).size() == blockCount);
  REQUIRE(arena.full());
}

PF_TEST_CASE("single threaded blocks smaller than the free list link still work",
             "[mem][poolArena][st]") {
  constexpr std::size_t tinyBlockSize = 4;
  constexpr std::size_t tinyCount = 4;
  constexpr std::size_t tinyAlignment = 8;
  using Storage = PoolStorage<tinyCount * tinyAlignment,
                              tinyBlockSize,
                              tinyCount,
                              tinyAlignment,
                              PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  REQUIRE(arena.block_capacity() == tinyCount);
  REQUIRE(arena.stride() == tinyAlignment);
  REQUIRE(arena.block_size() == tinyBlockSize);
  REQUIRE(arena.capacity() == tinyCount * tinyAlignment);

  auto blocks = drainSt(arena);
  REQUIRE(arena.full());
  REQUIRE(allDistinct(blocks));

  for (std::size_t i = 0; i < blocks.size(); i++) {
    REQUIRE(static_cast<std::size_t>(blocks[i] - storage.data()) % tinyAlignment == 0);
    tagBlock(blocks[i], i, tinyBlockSize);
  }
  // the link shares storage with a tiny block, but must not spill past the stride
  for (auto* block : blocks) {
    REQUIRE(block + tinyAlignment <= storage.data() + arena.capacity());
  }
  for (auto* block : blocks) {
    arena.deallocate(block);
  }
  REQUIRE_FALSE(arena.full());

  blocks = drainSt(arena);
  REQUIRE(arena.full());
  REQUIRE(allDistinct(blocks));
  for (auto* block : blocks) {
    arena.deallocate(block);
  }
}

PF_TEST_CASE("single threaded padded stride keeps every block aligned",
             "[mem][poolArena][st]") {
  constexpr std::size_t unalignedBlockSize = 32;
  constexpr std::size_t strictAlignment = 64;
  constexpr std::size_t count = 4;
  using Storage = PoolStorage<count * strictAlignment,
                              unalignedBlockSize,
                              count,
                              strictAlignment,
                              PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  REQUIRE(arena.block_capacity() == count);
  REQUIRE(arena.block_size() == unalignedBlockSize);
  REQUIRE(arena.stride() == strictAlignment);
  REQUIRE(arena.alignment() == strictAlignment);

  const auto blocks = drainSt(arena);
  for (auto* block : blocks) {
    REQUIRE(pointer_cast<std::uintptr_t>(block) % strictAlignment == 0);
    // the whole stride, not just the user block, must not overlap the next one
    REQUIRE(block + arena.stride() <= storage.data() + count * strictAlignment);
  }
  REQUIRE(arena.full());
  for (auto* block : blocks) {
    arena.deallocate(block);
  }
  REQUIRE_FALSE(arena.full());
}

PF_TEST_CASE("single threaded default constructed arena is null and empty",
             "[mem][poolArena][st]") {
  PoolArena arena;
  REQUIRE(arena.is_null());
  REQUIRE(arena.full());
  REQUIRE(arena.block_capacity() == 0);
  REQUIRE(arena.allocate().is_null());
}

PF_TEST_CASE("single threaded dealloc rejects pointers that are not block starts",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * block_size, block_size, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  const auto blocks = drainSt(arena);
  REQUIRE(blocks.size() == blockCount);
  REQUIRE(arena.full());

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  // a pointer into the middle of a block is not a free list link slot
  REQUIRE_PF_REQUIRE_FAIL(arena.deallocate(blocks[0] + 1));
  // nor is one past the end of the pool
  REQUIRE_PF_REQUIRE_FAIL(arena.deallocate(storage.data() + arena.capacity()));
  // the rejected calls must neither consume nor corrupt a block
  REQUIRE(arena.full());
  REQUIRE(arena.allocate().is_null());
#endif

  arena.deallocate(blocks[0]);
  REQUIRE_FALSE(arena.full());
  REQUIRE(arena.allocate().data == blocks[0]);
}

PF_TEST_CASE("single threaded create params are validated", "[mem][poolArena][st]") {
#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  {
    // the block size must be a power of two
    alignas(alignment) std::array<std::byte, block_size * blockCount> storage{};
    Buffer buffer{Buffer::from(static_cast<std::byte*>(storage.data()), storage.size())};
    PoolArena::CreateParams params{.buffer = buffer,
                                   .block_size = block_size - 1,
                                   .block_count = blockCount,
                                   .alignment = alignment};
    const auto makeArena = [&params]() { return PoolArena{params}; };
    REQUIRE_PF_REQUIRE_FAIL(makeArena());
  }

  {
    // the block count must be a power of two
    alignas(alignment) std::array<std::byte, block_size * blockCount> storage{};
    Buffer buffer{Buffer::from(static_cast<std::byte*>(storage.data()), storage.size())};
    PoolArena::CreateParams params{.buffer = buffer,
                                   .block_size = block_size,
                                   .block_count = blockCount - 1,
                                   .alignment = alignment};
    const auto makeArena = [&params]() { return PoolArena{params}; };
    REQUIRE_PF_REQUIRE_FAIL(makeArena());
  }

  {
    // the buffer must be exactly block_count * stride bytes
    alignas(alignment) std::array<std::byte, block_size * blockCount> storage{};
    Buffer buffer{
        Buffer::from(static_cast<std::byte*>(storage.data()), storage.size() - 1)};
    PoolArena::CreateParams params{.buffer = buffer,
                                   .block_size = block_size,
                                   .block_count = blockCount,
                                   .alignment = alignment};
    const auto makeArena = [&params]() { return PoolArena{params}; };
    REQUIRE_PF_REQUIRE_FAIL(makeArena());
  }

  {
    // the base pointer must satisfy the requested alignment
    alignas(alignment) std::array<std::byte, block_size * blockCount> storage{};
    Buffer buffer{Buffer::from(static_cast<std::byte*>(storage.data()) + 1,
                               block_size * blockCount)};
    PoolArena::CreateParams params{.buffer = buffer,
                                   .block_size = block_size,
                                   .block_count = blockCount,
                                   .alignment = alignment};
    const auto makeArena = [&params]() { return PoolArena{params}; };
    REQUIRE_PF_REQUIRE_FAIL(makeArena());
  }
#endif
}

PF_TEST_CASE("single threaded move construction keeps the pool and nulls the source",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * block_size, block_size, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  const Buffer held = arena.allocate();
  REQUIRE_FALSE(held.is_null());

  PoolArena moved{std::move(arena)};
  REQUIRE(arena.is_null());
  REQUIRE(arena.full());
  REQUIRE(arena.allocate().is_null());

  REQUIRE_FALSE(moved.is_null());
  REQUIRE_FALSE(moved.full());
  REQUIRE(moved.block_capacity() == blockCount);
  REQUIRE(moved.block_size() == block_size);
  REQUIRE(moved.stride() == block_size);
  REQUIRE(moved.alignment() == alignment);
  REQUIRE(moved.capacity() == blockCount * block_size);

  // the block held before the move is still owned, not handed out again
  const auto rest = drainSt(moved, blockCount - 1);
  REQUIRE(rest.size() == blockCount - 1);
  REQUIRE(std::find(rest.begin(), rest.end(), held.data) == rest.end());
  REQUIRE(moved.full());
  REQUIRE(moved.allocate().is_null());

  for (auto* block : rest) {
    moved.deallocate(block);
  }
}

PF_TEST_CASE("single threaded move assignment fills a null arena and rejects a live one",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * block_size, block_size, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  PoolArena target;
  REQUIRE(target.is_null());

  target = std::move(arena);
  REQUIRE(arena.is_null());
  REQUIRE_FALSE(target.is_null());
  REQUIRE(target.block_capacity() == blockCount);
  REQUIRE_FALSE(target.full());

  const auto blocks = drainSt(target);
  REQUIRE(blocks.size() == blockCount);
  for (auto* block : blocks) {
    target.deallocate(block);
  }

  PoolArena live{storage.params()};
#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  REQUIRE_PF_REQUIRE_FAIL(live = std::move(target));
  // the rejected move must leave the source fully usable
  REQUIRE_FALSE(target.is_null());
  REQUIRE_FALSE(target.full());
#endif
  live.deallocate(live.allocate().data);
  REQUIRE_FALSE(live.full());
}

PF_TEST_CASE("single threaded destruction does not demand a drained pool",
             "[mem][poolArena][st]") {
  // the arena does not own the blocks, so outstanding allocations are the
  // caller's business, destroying must not require or forbid any state
  for (std::size_t keep = 0; keep <= blockCount; keep++) {
    using Storage = PoolStorage<blockCount * block_size,
                                block_size,
                                blockCount,
                                alignment,
                                PoolArena>;
    Storage storage;
    PoolArena arena{storage.params()};
    for (std::size_t i = 0; i < keep; i++) {
      REQUIRE_FALSE(arena.allocate().is_null());
    }
  }
}

}
