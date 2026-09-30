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

constexpr std::size_t blockSize = 64;
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
 * @tparam T_Arena the arena the params belong to, i.e ConcurrentPoolArena or
 *         PoolArena
 */
template <std::size_t T_storageSize,
          std::size_t T_blockSize = blockSize,
          std::size_t T_blockCount = T_storageSize / strideOf(T_blockSize, alignment),
          std::size_t T_alignment = alignment,
          typename T_Arena = ConcurrentPoolArena>
class PoolStorage {
public:
  using CreateParams = typename T_Arena::CreateParams;

  static_assert(T_alignment != 0, "alignment must be non zero");
  static_assert(T_storageSize >= T_blockCount * strideOf(T_blockSize, T_alignment),
                "storage is too small for the requested blocks");

  PoolStorage()
    : m_buffer(Buffer::from(static_cast<std::byte*>(m_storage.data()), m_storage.size())),
      m_params{.buffer = m_buffer,
               .block_size = T_blockSize,
               .block_count = T_blockCount,
               .alignment = T_alignment} {}

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
      std::array<std::byte, T_storageSize> m_storage{};
  Buffer m_buffer{Buffer::null()};
  CreateParams m_params;
};

/**@brief Fills a whole block with a tag derived from the block index */
void
tagBlock(std::byte* block, std::size_t blockIdx, std::size_t size) {
  std::fill_n(block, size, static_cast<std::byte>(blockIdx + 1));
}

[[nodiscard]] bool
isTagged(const ConcurrentPoolArena::Slot& slot) {
  return std::all_of(slot.buffer.data,
                     slot.buffer.data + slot.buffer.size,
                     [idx = slot.blockIdx](std::byte byte) {
                       return byte == static_cast<std::byte>(idx + 1);
                     });
}

/**@brief Allocates the whole pool, returning the block indices in hand out order */
[[nodiscard]] std::vector<ConcurrentPoolArena::Slot>
drain(ConcurrentPoolArena& arena) {
  std::vector<ConcurrentPoolArena::Slot> slots;
  for (std::size_t i = 0; i < arena.blockCapacity(); i++) {
    const auto slot = arena.alloc();
    REQUIRE_FALSE(slot.isNull());
    slots.push_back(slot);
  }
  return slots;
}

[[nodiscard]] std::vector<std::size_t>
sortedIndices(const std::vector<ConcurrentPoolArena::Slot>& slots) {
  std::vector<std::size_t> indices;
  indices.reserve(slots.size());
  for (const auto& slot : slots) {
    indices.push_back(slot.blockIdx);
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
    const Buffer block = arena.alloc();
    REQUIRE_FALSE(block.isNull());
    blocks.push_back(block.data);
  }
  return blocks;
}

/**@brief Allocates the whole pool of a single threaded arena */
[[nodiscard]] std::vector<std::byte*>
drainSt(PoolArena& arena) {
  return drainSt(arena, arena.blockCapacity());
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
  PoolStorage<blockCount * blockSize> storage;
  ConcurrentPoolArena arena{storage.params()};

  REQUIRE_FALSE(arena.isNull());
  REQUIRE(arena.blockCapacity() == blockCount);
  REQUIRE(arena.blockSize() == blockSize);
  REQUIRE(arena.stride() == blockSize);
  REQUIRE(arena.alignment() == alignment);
  REQUIRE_FALSE(arena.full());
  REQUIRE(arena.resolve(0) == storage.data());
  REQUIRE(arena.resolve(blockCount - 1) == storage.data() + (blockCount - 1) * blockSize);
}

PF_TEST_CASE("alloc hands out distinct, tagged, in range blocks", "[mem][poolArena]") {
  PoolStorage<blockCount * blockSize> storage;
  ConcurrentPoolArena arena{storage.params()};

  const auto slots = drain(arena);

  for (std::size_t i = 0; i < blockCount; i++) {
    const auto& slot = slots[i];
    REQUIRE_FALSE(slot.buffer.isNull());
    REQUIRE(slot.buffer.size == blockSize);
    REQUIRE(slot.buffer.data == arena.resolve(slot.blockIdx));
    REQUIRE(slot.buffer.data >= storage.data());
    REQUIRE(slot.buffer.data + blockSize <= storage.data() + blockSize * blockCount);
  }

  for (std::size_t i = 0; i < blockCount; i++) {
    tagBlock(slots[i].buffer.data, slots[i].blockIdx, blockSize);
  }

  for (std::size_t i = 0; i < blockCount; i++) {
    for (std::size_t j = i + 1; j < blockCount; j++) {
      REQUIRE(slots[i].blockIdx != slots[j].blockIdx);
      REQUIRE(slots[i].buffer.data != slots[j].buffer.data);
    }
    // no block may have been clobbered by a later allocation
    REQUIRE(isTagged(slots[i]));
  }

  for (const auto& slot : slots) {
    arena.dealloc(slot.blockIdx);
  }
}

PF_TEST_CASE("alloc yields a null slot once the pool is exhausted", "[mem][poolArena]") {
  PoolStorage<blockCount * blockSize> storage;
  ConcurrentPoolArena arena{storage.params()};

  const auto slots = drain(arena);
  REQUIRE(arena.full());

  const auto exhausted = arena.alloc();
  REQUIRE(exhausted.isNull());
  REQUIRE(exhausted.buffer.isNull());
  REQUIRE(arena.full());

  // every block must have been handed out exactly once
  const auto indices = sortedIndices(slots);
  REQUIRE(std::adjacent_find(indices.begin(), indices.end()) == indices.end());
  std::vector<std::size_t> expected(blockCount);
  std::iota(expected.begin(), expected.end(), std::size_t{0});
  REQUIRE(indices == expected);

  for (const auto& slot : slots) {
    arena.dealloc(slot.blockIdx);
  }
}

PF_TEST_CASE("dealloc returns the block and the pool reports free capacity",
             "[mem][poolArena]") {
  PoolStorage<blockCount * blockSize> storage;
  ConcurrentPoolArena arena{storage.params()};

  const auto slots = drain(arena);
  REQUIRE(arena.full());

  constexpr std::size_t freed = 3;
  arena.dealloc(slots[freed].blockIdx);
  REQUIRE_FALSE(arena.full());

  const auto reused = arena.alloc();
  REQUIRE_FALSE(reused.isNull());
  REQUIRE(reused.blockIdx == slots[freed].blockIdx);
  REQUIRE(reused.buffer.data == slots[freed].buffer.data);
  REQUIRE(arena.full());

  for (const auto& slot : slots) {
    if (slot.blockIdx != freed) {
      arena.dealloc(slot.blockIdx);
    }
  }
  REQUIRE_FALSE(arena.full());
}

PF_TEST_CASE("free blocks are handed back out last in, first out", "[mem][poolArena]") {
  PoolStorage<blockCount * blockSize> storage;
  ConcurrentPoolArena arena{storage.params()};

  const auto slots = drain(arena);

  std::vector<std::size_t> freed;
  for (std::size_t i = slots.size(); i > 0; i--) {
    arena.dealloc(slots[i - 1].blockIdx);
    freed.push_back(slots[i - 1].blockIdx);
  }
  REQUIRE_FALSE(arena.full());

  // the free list is a stack, so the block freed last comes out first
  for (auto it = freed.rbegin(); it != freed.rend(); ++it) {
    const auto slot = arena.alloc();
    REQUIRE_FALSE(slot.isNull());
    REQUIRE(slot.blockIdx == *it);
  }
  REQUIRE(arena.full());
}

PF_TEST_CASE("dealloc by pointer matches dealloc by index", "[mem][poolArena]") {
  PoolStorage<blockCount * blockSize> storage;
  ConcurrentPoolArena arena{storage.params()};

  const auto slots = drain(arena);

  arena.dealloc(arena.resolve(slots[2].blockIdx));
  arena.dealloc(slots[4].buffer.data);
  REQUIRE_FALSE(arena.full());

  const auto first = arena.alloc();
  const auto second = arena.alloc();
  REQUIRE(first.blockIdx == slots[4].blockIdx);
  REQUIRE(second.blockIdx == slots[2].blockIdx);
  REQUIRE(arena.full());

  for (const auto& slot : slots) {
    if (slot.blockIdx != 2 && slot.blockIdx != 4) {
      arena.dealloc(slot.blockIdx);
    }
  }
  REQUIRE_FALSE(arena.full());
}

PF_TEST_CASE("churning the pool never loses or duplicates a block", "[mem][poolArena]") {
  PoolStorage<blockCount * blockSize> storage;
  ConcurrentPoolArena arena{storage.params()};

  for (std::size_t round = 0; round < 32; round++) {
    const auto slots = drain(arena);
    REQUIRE(arena.full());

    const auto indices = sortedIndices(slots);
    REQUIRE(std::adjacent_find(indices.begin(), indices.end()) == indices.end());
    REQUIRE(indices.front() == 0);
    REQUIRE(indices.back() == blockCount - 1);

    for (const auto idx : indices) {
      arena.dealloc(idx);
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

  REQUIRE(arena.blockCapacity() == tinyCount);
  REQUIRE(arena.stride() == tinyAlignment);
  REQUIRE(arena.blockSize() == tinyBlockSize);

  auto slots = drain(arena);
  REQUIRE(arena.full());

  for (auto& slot : slots) {
    REQUIRE(slot.buffer.size == tinyBlockSize);
    tagBlock(slot.buffer.data, slot.blockIdx, tinyBlockSize);
  }
  for (const auto& slot : slots) {
    REQUIRE(isTagged(slot));
    arena.dealloc(slot.blockIdx);
  }
  REQUIRE_FALSE(arena.full());

  slots = drain(arena);
  REQUIRE(arena.full());
  for (const auto& slot : slots) {
    arena.dealloc(slot.blockIdx);
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

  REQUIRE(arena.blockCapacity() == count);
  REQUIRE(arena.blockSize() == unalignedBlockSize);
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
    arena.dealloc(slot.blockIdx);
  }
}

PF_TEST_CASE("a default constructed arena is null and empty", "[mem][poolArena]") {
  ConcurrentPoolArena arena;
  REQUIRE(arena.isNull());
  REQUIRE(arena.full());
  REQUIRE(arena.alloc().isNull());
}

PF_TEST_CASE("destruction does not demand a drained pool", "[mem][poolArena]") {
  // the arena does not own the blocks, so outstanding allocations are the
  // caller's business, destroying must not require or forbid any state
  for (std::size_t keep = 0; keep <= blockCount; keep++) {
    PoolStorage<blockCount * blockSize> storage;
    ConcurrentPoolArena arena{storage.params()};
    for (std::size_t i = 0; i < keep; i++) {
      REQUIRE_FALSE(arena.alloc().isNull());
    }
  }
}

PF_TEST_CASE("resolve rejects indices outside the pool", "[mem][poolArena]") {
  PoolStorage<blockCount * blockSize> storage;
  ConcurrentPoolArena arena{storage.params()};

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  REQUIRE_PF_REQUIRE_FAIL(arena.resolve(arena.blockCapacity()));
  REQUIRE_PF_REQUIRE_FAIL(arena.resolve(std::numeric_limits<std::size_t>::max()));
#endif

  const auto slot = arena.alloc();
  REQUIRE(arena.resolve(slot) == slot.buffer.data);
  arena.dealloc(slot.blockIdx);
}

PF_TEST_CASE("create params are validated", "[mem][poolArena]") {
#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  {
    // the block size must be a power of two
    alignas(alignment) std::array<std::byte, blockSize * blockCount> storage{};
    Buffer buffer{Buffer::from(static_cast<std::byte*>(storage.data()), storage.size())};
    ConcurrentPoolArena::CreateParams params{.buffer = buffer,
                                             .block_size = blockSize - 1,
                                             .block_count = blockCount,
                                             .alignment = alignment};
    const auto makeArena = [&params]() { return ConcurrentPoolArena{params}; };
    REQUIRE_PF_REQUIRE_FAIL(makeArena());
  }

  {
    // the buffer must be exactly block_count * stride bytes
    alignas(alignment) std::array<std::byte, blockSize * blockCount> storage{};
    Buffer buffer{
        Buffer::from(static_cast<std::byte*>(storage.data()), storage.size() - 1)};
    ConcurrentPoolArena::CreateParams params{.buffer = buffer,
                                             .block_size = blockSize,
                                             .block_count = blockCount,
                                             .alignment = alignment};
    const auto makeArena = [&params]() { return ConcurrentPoolArena{params}; };
    REQUIRE_PF_REQUIRE_FAIL(makeArena());
  }

  {
    // the base pointer must satisfy the requested alignment
    alignas(alignment) std::array<std::byte, blockSize * blockCount> storage{};
    Buffer buffer{Buffer::from(static_cast<std::byte*>(storage.data()) + 1,
                               blockSize * blockCount)};
    ConcurrentPoolArena::CreateParams params{.buffer = buffer,
                                             .block_size = blockSize,
                                             .block_count = blockCount,
                                             .alignment = alignment};
    const auto makeArena = [&params]() { return ConcurrentPoolArena{params}; };
    REQUIRE_PF_REQUIRE_FAIL(makeArena());
  }
#endif
}

PF_TEST_CASE("move construction keeps the pool and nulls the source",
             "[mem][poolArena]") {
  PoolStorage<blockCount * blockSize> storage;
  ConcurrentPoolArena arena{storage.params()};

  const auto held = arena.alloc();
  REQUIRE_FALSE(held.isNull());

  ConcurrentPoolArena moved{std::move(arena)};
  REQUIRE(arena.isNull());
  REQUIRE(arena.full());
  REQUIRE(arena.alloc().isNull());

  REQUIRE_FALSE(moved.isNull());
  REQUIRE(moved.blockCapacity() == blockCount);
  REQUIRE(moved.blockSize() == blockSize);
  REQUIRE(moved.alignment() == alignment);
  // the block held before the move is still owned, not handed out again
  REQUIRE(moved.resolve(held.blockIdx) == held.buffer.data);

  const auto next = moved.alloc();
  REQUIRE_FALSE(next.isNull());
  REQUIRE(next.blockIdx != held.blockIdx);

  moved.dealloc(next.blockIdx);
  moved.dealloc(held.blockIdx);
}

PF_TEST_CASE("move assignment fills a null arena and rejects a live one",
             "[mem][poolArena]") {
  PoolStorage<blockCount * blockSize> storage;
  ConcurrentPoolArena arena{storage.params()};

  ConcurrentPoolArena target;
  REQUIRE(target.isNull());

  target = std::move(arena);
  REQUIRE(arena.isNull());
  REQUIRE_FALSE(target.isNull());
  REQUIRE(target.blockCapacity() == blockCount);
  REQUIRE_FALSE(target.full());

  const auto slot = target.alloc();
  REQUIRE_FALSE(slot.isNull());
  target.dealloc(slot.blockIdx);

  ConcurrentPoolArena live{storage.params()};
#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  REQUIRE_PF_REQUIRE_FAIL(live = std::move(target));
#endif
  live.dealloc(live.alloc().blockIdx);
}

PF_TEST_CASE("concurrent alloc dealloc churn never double hands a block out",
             "[mem][poolArena]") {
  constexpr std::size_t threadCount = 8;
  constexpr std::size_t iterations = 2000;

  PoolStorage<blockCount * blockSize> storage;
  ConcurrentPoolArena arena{storage.params()};

  std::array<std::atomic<std::uint32_t>, blockCount> claims{};
  std::atomic<std::size_t> doubleClaims{0};
  std::atomic<std::size_t> nullAllocs{0};
  std::atomic<std::size_t> successfulAllocs{0};

  const auto worker = [&arena, &claims, &doubleClaims, &nullAllocs, &successfulAllocs]() {
    for (std::size_t i = 0; i < iterations; i++) {
      const auto slot = arena.alloc();
      if (slot.isNull()) {
        nullAllocs.fetch_add(1, std::memory_order::relaxed);
        continue;
      }
      successfulAllocs.fetch_add(1, std::memory_order::relaxed);
      if (claims[slot.blockIdx].fetch_add(1, std::memory_order::acq_rel) != 0) {
        doubleClaims.fetch_add(1, std::memory_order::relaxed);
      }
      claims[slot.blockIdx].fetch_sub(1, std::memory_order::acq_rel);
      arena.dealloc(slot.buffer.data);
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
    arena.dealloc(idx);
  }
}

PF_TEST_CASE("concurrent hand out gives every block to exactly one thread",
             "[mem][poolArena]") {
  constexpr std::size_t threadCount = blockCount;

  PoolStorage<blockCount * blockSize> storage;
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
    const auto slot = arena.alloc();
    if (slot.isNull()) {
      return;
    }
    if (!claimed[slot.blockIdx].exchange(true, std::memory_order::acq_rel)) {
      winners.fetch_add(1, std::memory_order::relaxed);
      taken[id].push_back(slot.blockIdx);
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
    arena.dealloc(idx);
  }
  REQUIRE_FALSE(arena.full());
}

//----------------------------------------------------------------------------
// pf::mem::PoolArena, the single threaded pool arena
//----------------------------------------------------------------------------

PF_TEST_CASE("single threaded arena exposes the geometry it was built with",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * blockSize, blockSize, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  REQUIRE_FALSE(arena.isNull());
  REQUIRE_FALSE(arena.full());
  REQUIRE(arena.blockCapacity() == blockCount);
  REQUIRE(arena.blockSize() == blockSize);
  REQUIRE(arena.stride() == blockSize);
  REQUIRE(arena.alignment() == alignment);
  REQUIRE(arena.capacity() == blockCount * blockSize);
}

PF_TEST_CASE("single threaded alloc hands out distinct, in range blocks",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * blockSize, blockSize, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  const auto blocks = drainSt(arena);
  REQUIRE(blocks.size() == blockCount);
  REQUIRE(allDistinct(blocks));

  for (auto* block : blocks) {
    REQUIRE(block >= storage.data());
    REQUIRE(block + blockSize <= storage.data() + blockCount * blockSize);
    // a block must start on a stride boundary inside the pool
    REQUIRE(static_cast<std::size_t>(block - storage.data()) % arena.stride() == 0);
  }

  // every block of the pool must have been handed out, exactly once
  const auto indices = sortedIndicesSt(blocks, arena, storage.data());
  std::vector<std::size_t> expected(blockCount);
  std::iota(expected.begin(), expected.end(), std::size_t{0});
  REQUIRE(indices == expected);

  for (auto* block : blocks) {
    arena.dealloc(block);
  }
}

PF_TEST_CASE(
    "single threaded alloc reports the requested block size and keeps payloads intact",
    "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * blockSize, blockSize, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  // hold a single block back, tag everything else, then free it
  const Buffer held = arena.alloc();
  REQUIRE_FALSE(held.isNull());
  REQUIRE(held.size == arena.blockSize());

  std::vector<std::byte*> tagged;
  while (!arena.full()) {
    const Buffer block = arena.alloc();
    REQUIRE_FALSE(block.isNull());
    REQUIRE(block.size == blockSize);
    tagBlock(block.data, tagged.size(), block.size);
    tagged.push_back(block.data);
  }
  REQUIRE(tagged.size() == blockCount - 1);

  arena.dealloc(held.data);
  const Buffer reused = arena.alloc();
  REQUIRE(reused.data == held.data);

  // a later allocation must never clobber an earlier block
  for (std::size_t i = 0; i < tagged.size(); i++) {
    REQUIRE(std::all_of(
        tagged[i],
        tagged[i] + blockSize,
        [tag = static_cast<std::byte>(i + 1)](std::byte byte) { return byte == tag; }));
  }

  for (auto* block : tagged) {
    arena.dealloc(block);
  }
  arena.dealloc(reused.data);
  REQUIRE_FALSE(arena.full());
}

PF_TEST_CASE("single threaded alloc yields a null buffer once exhausted",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * blockSize, blockSize, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  const auto blocks = drainSt(arena);
  REQUIRE(arena.full());

  const Buffer exhausted = arena.alloc();
  REQUIRE(exhausted.isNull());
  REQUIRE(exhausted.data == nullptr);
  REQUIRE(exhausted.size == 0);
  REQUIRE(arena.full());

  // an exhausted pool must stay exhausted and must not lose any block
  for (std::size_t i = 0; i < 3; i++) {
    REQUIRE(arena.alloc().isNull());
  }
  REQUIRE(arena.full());

  for (auto* block : blocks) {
    arena.dealloc(block);
  }
  REQUIRE_FALSE(arena.full());
  REQUIRE(drainSt(arena).size() == blockCount);
}

PF_TEST_CASE(
    "single threaded dealloc returns the block and the pool reports free capacity",
    "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * blockSize, blockSize, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  const auto blocks = drainSt(arena);
  REQUIRE(arena.full());

  constexpr std::size_t freed = 3;
  arena.dealloc(blocks[freed]);
  REQUIRE_FALSE(arena.full());

  const Buffer reused = arena.alloc();
  REQUIRE_FALSE(reused.isNull());
  REQUIRE(reused.data == blocks[freed]);
  REQUIRE(reused.size == blockSize);
  REQUIRE(arena.full());

  for (std::size_t i = 0; i < blocks.size(); i++) {
    if (i != freed) {
      arena.dealloc(blocks[i]);
    }
  }
  // the block handed back out above is still held
  arena.dealloc(reused.data);
  REQUIRE_FALSE(arena.full());
  REQUIRE(drainSt(arena).size() == blockCount);
}

PF_TEST_CASE("single threaded free blocks are handed back out last in, first out",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * blockSize, blockSize, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  const auto blocks = drainSt(arena);

  std::vector<std::byte*> freed;
  for (std::size_t i = blocks.size(); i > 0; i--) {
    arena.dealloc(blocks[i - 1]);
    freed.push_back(blocks[i - 1]);
  }
  REQUIRE_FALSE(arena.full());

  // the free list is a stack, so the block freed last comes out first
  for (auto it = freed.rbegin(); it != freed.rend(); ++it) {
    const Buffer block = arena.alloc();
    REQUIRE_FALSE(block.isNull());
    REQUIRE(block.data == *it);
  }
  REQUIRE(arena.full());
  REQUIRE(arena.alloc().isNull());
}

PF_TEST_CASE("single threaded churn never loses or duplicates a block",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * blockSize, blockSize, blockCount, alignment, PoolArena>;
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
      arena.dealloc(block);
    }
    REQUIRE_FALSE(arena.full());
  }
}

PF_TEST_CASE("single threaded interleaved alloc and dealloc keeps the pool leak free",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * blockSize, blockSize, blockCount, alignment, PoolArena>;
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
      const Buffer block = arena.alloc();
      if (block.isNull()) {
        REQUIRE(live.size() == blockCount);
        REQUIRE(arena.full());
      } else {
        REQUIRE(block.size == blockSize);
        live.push_back(block.data);
        REQUIRE(live.size() <= blockCount);
      }
    } else {
      const auto victim = nextRandom() % live.size();
      arena.dealloc(live[victim]);
      live.erase(live.begin() + static_cast<std::ptrdiff_t>(victim));
      REQUIRE_FALSE(arena.full());
    }

    REQUIRE(allDistinct(live));
    REQUIRE(arena.full() == (live.size() == blockCount));
  }

  for (auto* block : live) {
    arena.dealloc(block);
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

  REQUIRE(arena.blockCapacity() == tinyCount);
  REQUIRE(arena.stride() == tinyAlignment);
  REQUIRE(arena.blockSize() == tinyBlockSize);
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
    arena.dealloc(block);
  }
  REQUIRE_FALSE(arena.full());

  blocks = drainSt(arena);
  REQUIRE(arena.full());
  REQUIRE(allDistinct(blocks));
  for (auto* block : blocks) {
    arena.dealloc(block);
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

  REQUIRE(arena.blockCapacity() == count);
  REQUIRE(arena.blockSize() == unalignedBlockSize);
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
    arena.dealloc(block);
  }
  REQUIRE_FALSE(arena.full());
}

PF_TEST_CASE("single threaded default constructed arena is null and empty",
             "[mem][poolArena][st]") {
  PoolArena arena;
  REQUIRE(arena.isNull());
  REQUIRE(arena.full());
  REQUIRE(arena.blockCapacity() == 0);
  REQUIRE(arena.alloc().isNull());
}

PF_TEST_CASE("single threaded dealloc rejects pointers that are not block starts",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * blockSize, blockSize, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  const auto blocks = drainSt(arena);
  REQUIRE(blocks.size() == blockCount);
  REQUIRE(arena.full());

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  // a pointer into the middle of a block is not a free list link slot
  REQUIRE_PF_REQUIRE_FAIL(arena.dealloc(blocks[0] + 1));
  // nor is one past the end of the pool
  REQUIRE_PF_REQUIRE_FAIL(arena.dealloc(storage.data() + arena.capacity()));
  // the rejected calls must neither consume nor corrupt a block
  REQUIRE(arena.full());
  REQUIRE(arena.alloc().isNull());
#endif

  arena.dealloc(blocks[0]);
  REQUIRE_FALSE(arena.full());
  REQUIRE(arena.alloc().data == blocks[0]);
}

PF_TEST_CASE("single threaded create params are validated", "[mem][poolArena][st]") {
#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  {
    // the block size must be a power of two
    alignas(alignment) std::array<std::byte, blockSize * blockCount> storage{};
    Buffer buffer{Buffer::from(static_cast<std::byte*>(storage.data()), storage.size())};
    PoolArena::CreateParams params{.buffer = buffer,
                                   .block_size = blockSize - 1,
                                   .block_count = blockCount,
                                   .alignment = alignment};
    const auto makeArena = [&params]() { return PoolArena{params}; };
    REQUIRE_PF_REQUIRE_FAIL(makeArena());
  }

  {
    // the block count must be a power of two
    alignas(alignment) std::array<std::byte, blockSize * blockCount> storage{};
    Buffer buffer{Buffer::from(static_cast<std::byte*>(storage.data()), storage.size())};
    PoolArena::CreateParams params{.buffer = buffer,
                                   .block_size = blockSize,
                                   .block_count = blockCount - 1,
                                   .alignment = alignment};
    const auto makeArena = [&params]() { return PoolArena{params}; };
    REQUIRE_PF_REQUIRE_FAIL(makeArena());
  }

  {
    // the buffer must be exactly block_count * stride bytes
    alignas(alignment) std::array<std::byte, blockSize * blockCount> storage{};
    Buffer buffer{
        Buffer::from(static_cast<std::byte*>(storage.data()), storage.size() - 1)};
    PoolArena::CreateParams params{.buffer = buffer,
                                   .block_size = blockSize,
                                   .block_count = blockCount,
                                   .alignment = alignment};
    const auto makeArena = [&params]() { return PoolArena{params}; };
    REQUIRE_PF_REQUIRE_FAIL(makeArena());
  }

  {
    // the base pointer must satisfy the requested alignment
    alignas(alignment) std::array<std::byte, blockSize * blockCount> storage{};
    Buffer buffer{Buffer::from(static_cast<std::byte*>(storage.data()) + 1,
                               blockSize * blockCount)};
    PoolArena::CreateParams params{.buffer = buffer,
                                   .block_size = blockSize,
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
      PoolStorage<blockCount * blockSize, blockSize, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  const Buffer held = arena.alloc();
  REQUIRE_FALSE(held.isNull());

  PoolArena moved{std::move(arena)};
  REQUIRE(arena.isNull());
  REQUIRE(arena.full());
  REQUIRE(arena.alloc().isNull());

  REQUIRE_FALSE(moved.isNull());
  REQUIRE_FALSE(moved.full());
  REQUIRE(moved.blockCapacity() == blockCount);
  REQUIRE(moved.blockSize() == blockSize);
  REQUIRE(moved.stride() == blockSize);
  REQUIRE(moved.alignment() == alignment);
  REQUIRE(moved.capacity() == blockCount * blockSize);

  // the block held before the move is still owned, not handed out again
  const auto rest = drainSt(moved, blockCount - 1);
  REQUIRE(rest.size() == blockCount - 1);
  REQUIRE(std::find(rest.begin(), rest.end(), held.data) == rest.end());
  REQUIRE(moved.full());
  REQUIRE(moved.alloc().isNull());

  for (auto* block : rest) {
    moved.dealloc(block);
  }
}

PF_TEST_CASE("single threaded move assignment fills a null arena and rejects a live one",
             "[mem][poolArena][st]") {
  using Storage =
      PoolStorage<blockCount * blockSize, blockSize, blockCount, alignment, PoolArena>;
  Storage storage;
  PoolArena arena{storage.params()};

  PoolArena target;
  REQUIRE(target.isNull());

  target = std::move(arena);
  REQUIRE(arena.isNull());
  REQUIRE_FALSE(target.isNull());
  REQUIRE(target.blockCapacity() == blockCount);
  REQUIRE_FALSE(target.full());

  const auto blocks = drainSt(target);
  REQUIRE(blocks.size() == blockCount);
  for (auto* block : blocks) {
    target.dealloc(block);
  }

  PoolArena live{storage.params()};
#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  REQUIRE_PF_REQUIRE_FAIL(live = std::move(target));
  // the rejected move must leave the source fully usable
  REQUIRE_FALSE(target.isNull());
  REQUIRE_FALSE(target.full());
#endif
  live.dealloc(live.alloc().data);
  REQUIRE_FALSE(live.full());
}

PF_TEST_CASE("single threaded destruction does not demand a drained pool",
             "[mem][poolArena][st]") {
  // the arena does not own the blocks, so outstanding allocations are the
  // caller's business, destroying must not require or forbid any state
  for (std::size_t keep = 0; keep <= blockCount; keep++) {
    using Storage =
        PoolStorage<blockCount * blockSize, blockSize, blockCount, alignment, PoolArena>;
    Storage storage;
    PoolArena arena{storage.params()};
    for (std::size_t i = 0; i < keep; i++) {
      REQUIRE_FALSE(arena.alloc().isNull());
    }
  }
}

}
