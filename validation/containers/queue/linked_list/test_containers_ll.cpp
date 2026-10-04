#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory_resource>
#include <optional>
#include <type_traits>
#include <utility>

#include <PixelForgeValidationHelpers/helpers.hpp>
#include <catch2/catch_test_macros.hpp>

import PixelForge.containers;
import PixelForge.core;
import PixelForge.validation_helpers;

namespace pf::containers {

namespace {

/**
 *@brief bookkeeping every test resource shares with the test that owns it
 */
struct AllocStats {
  std::size_t live_blocks{0};
  std::size_t allocations{0};
  std::size_t deallocations{0};
};

/**
 *@brief a memory resource that hands out aligned blocks and counts the ones
 *  still alive
 *
 *@note the stats are held behind a pointer so that the test can still see
 *  them once the queue holds its own copy of the resource
 */
class CountingResource {
public:
  static constexpr bool is_noexcept = true;

  /** a resource reporting itself null, it never manages a block */
  CountingResource() noexcept = default;

  CountingResource(AllocStats& stats, std::size_t id = 0) noexcept
    : m_stats(&stats), m_id(id) {}

  [[nodiscard]] bool
  is_null() const noexcept {
    return m_stats == nullptr;
  }

  [[nodiscard]] Buffer
  allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) {
    m_stats->allocations++;
    void* const ptr = std::aligned_alloc(alignment, size);
    if (ptr == nullptr) {
      return Buffer::null();
    }
    m_stats->live_blocks++;
    return Buffer::from(pointer_cast<std::byte*>(ptr), size);
  }

  void
  deallocate(const Buffer& buffer) {
    m_stats->deallocations++;
    m_stats->live_blocks--;
    std::free(buffer.data);
  }

  std::size_t
  id() const noexcept {
    return m_id;
  }

private:
  AllocStats* m_stats{nullptr};
  std::size_t m_id;
};

/**
 *@brief a memory resource that never manages to hand out a block
 */
class ExhaustedResource {
public:
  explicit ExhaustedResource(AllocStats& stats) noexcept : m_stats(&stats) {}

  [[nodiscard]] Buffer
  allocate(std::size_t, std::size_t = alignof(std::max_align_t)) {
    m_stats->allocations++;
    return Buffer::null();
  }

  void
  deallocate(const Buffer&) {}

private:
  AllocStats* m_stats;
};

struct MoveOnlyValue {
  std::uint32_t value{0};

  constexpr explicit MoveOnlyValue(std::uint32_t init = 0) : value(init) {}

  MoveOnlyValue(const MoveOnlyValue&) = delete;
  MoveOnlyValue&
  operator=(const MoveOnlyValue&) = delete;

  constexpr MoveOnlyValue(MoveOnlyValue&&) noexcept = default;
  constexpr MoveOnlyValue&
  operator=(MoveOnlyValue&&) noexcept = default;
};

using IntQueue = LLQueue<std::uint32_t, CountingResource>;
using MoveOnlyQueue = LLQueue<MoveOnlyValue, CountingResource>;

} // namespace

static_assert(mem::Resource_c<CountingResource>);
static_assert(mem::Resource_c<ExhaustedResource>);
static_assert(!mem::is_noexcept_resource<ExhaustedResource>::value);
static_assert(mem::is_noexcept_resource<CountingResource>::value);

PF_TEST_CASE("construction and type traits", "[containers][containers::LLQueue]") {
  SECTION("Traits") {
    using Queue = IntQueue;
    using Node = Queue::storage_type;

    static_assert(std::is_same_v<Queue::value_type, std::uint32_t>);
    static_assert(std::is_same_v<Queue::size_type, std::size_t>);
    static_assert(std::is_same_v<Queue::difference_type, std::ptrdiff_t>);
    static_assert(std::is_same_v<Queue::reference, std::uint32_t&>);
    static_assert(std::is_same_v<Queue::const_reference, const std::uint32_t&>);
    static_assert(std::is_same_v<Queue::pointer, std::uint32_t*>);
    static_assert(std::is_same_v<Queue::const_pointer, const std::uint32_t*>);
    static_assert(std::is_same_v<Queue::storage_type, Node>);
    static_assert(std::is_same_v<Queue::resource_type, CountingResource>);
    static_assert(std::is_same_v<decltype(Node::next), Node*>);
    static_assert(Queue::Traits::is_nothrow_allocate_v);
    static_assert(
        std::is_same_v<pf::containers::LLQueue<std::uint32_t, CountingResource>, Queue>);
    static_assert(!std::is_default_constructible_v<Queue>);
    static_assert(!std::is_copy_constructible_v<Queue>);
    static_assert(!std::is_copy_assignable_v<Queue>);
    static_assert(!std::is_move_constructible_v<Queue>);
    static_assert(!std::is_move_assignable_v<Queue>);
  }

  SECTION("a fresh queue is empty and has allocated nothing") {
    AllocStats stats;
    IntQueue queue{CountingResource(stats)};

    REQUIRE(queue.empty());
    REQUIRE(queue.size() == 0);
    REQUIRE(stats.live_blocks == 0);
    REQUIRE(!queue.try_pop().has_value());
  }

  SECTION("the resource is held by value") {
    AllocStats stats;
    CountingResource resource(stats, 7);
    IntQueue queue(resource);

    queue.emplace(42);

    // the queue allocates through its own copy of the resource
    REQUIRE(stats.live_blocks == 1);
    REQUIRE(stats.allocations == 1);
    REQUIRE(queue.resource().id() == 7);

    REQUIRE(queue.pop() == 42);
    REQUIRE(stats.live_blocks == 0);
  }

  SECTION("a resource can be handed over as an rvalue") {
    AllocStats stats;
    IntQueue queue{CountingResource(stats)};

    queue.emplace(1);
    REQUIRE(stats.live_blocks == 1);
    REQUIRE(queue.pop_unchecked() == 1);
    REQUIRE(stats.live_blocks == 0);
  }
}

PF_TEST_CASE("FIFO order and automatic node reclamation",
             "[containers][containers::LLQueue]") {
  AllocStats stats;
  IntQueue queue{CountingResource(stats)};

  constexpr std::size_t count = 8;
  for (std::size_t i = 0; i < count; ++i) {
    queue.emplace(static_cast<std::uint32_t>(i + 10));
    REQUIRE(queue.size() == i + 1);
    REQUIRE(!queue.empty());
    REQUIRE(stats.live_blocks == i + 1);
  }

  REQUIRE(queue.front() == 10);
  REQUIRE(queue.back() == 17);
  REQUIRE(stats.allocations == count);

  for (std::size_t i = 0; i < count; ++i) {
    const auto expected = static_cast<std::uint32_t>(i + 10);

    if (i % 2 == 0) {
      auto value = queue.try_pop();
      REQUIRE(value.has_value());
      REQUIRE(*value == expected);
    } else {
      REQUIRE(queue.pop() == expected);
    }

    REQUIRE(queue.size() == count - i - 1);
    // each pop hands its node back to the resource
    REQUIRE(stats.live_blocks == count - i - 1);
  }

  REQUIRE(queue.empty());
  REQUIRE(!queue.try_pop().has_value());
  REQUIRE(stats.deallocations == count);
  REQUIRE(stats.live_blocks == 0);
}

PF_TEST_CASE("destructor returns every node", "[containers][containers::LLQueue]") {
  AllocStats stats;

  {
    IntQueue queue{CountingResource(stats)};
    queue.emplace(1);
    queue.emplace(2);
    queue.emplace(3);
    REQUIRE(stats.live_blocks == 3);
  }

  REQUIRE(stats.live_blocks == 0);
  REQUIRE(stats.deallocations == 3);
}

PF_TEST_CASE("clear returns every node", "[containers][containers::LLQueue]") {
  AllocStats stats;
  IntQueue queue{CountingResource(stats)};

  queue.emplace(1);
  queue.emplace(2);
  REQUIRE(stats.live_blocks == 2);

  queue.clear();
  REQUIRE(queue.empty());
  REQUIRE(queue.size() == 0);
  REQUIRE(stats.live_blocks == 0);

  // the queue stays usable after a clear
  queue.emplace(3);
  REQUIRE(queue.size() == 1);
  REQUIRE(queue.pop_unchecked() == 3);
  REQUIRE(stats.live_blocks == 0);
}

PF_TEST_CASE("push and emplace overloads", "[containers][containers::LLQueue]") {
  SECTION("copy push preserves the source value") {
    AllocStats stats;
    IntQueue queue{CountingResource(stats)};

    std::uint32_t source = 17;
    queue.push(source);
    source = 99;

    REQUIRE(queue.pop() == 17);
    REQUIRE(stats.live_blocks == 0);
  }

  SECTION("rvalue push accepts a moved value") {
    AllocStats stats;
    IntQueue queue{CountingResource(stats)};

    std::uint32_t source = 23;
    queue.push(std::move(source));

    REQUIRE(queue.try_pop().value() == 23);
  }

  SECTION("try_emplace reports success") {
    AllocStats stats;
    IntQueue queue{CountingResource(stats)};

    REQUIRE(queue.try_emplace(31));
    REQUIRE(queue.size() == 1);
    REQUIRE(queue.pop_unchecked() == 31);
  }

  SECTION("move-only values can be queued") {
    AllocStats stats;
    MoveOnlyQueue queue{CountingResource(stats)};

    queue.push(MoveOnlyValue{41});
    queue.emplace(MoveOnlyValue{43});

    REQUIRE(queue.front().value == 41);
    REQUIRE(queue.back().value == 43);
    REQUIRE(queue.pop().value == 41);
    REQUIRE(queue.pop().value == 43);
    REQUIRE(stats.live_blocks == 0);
  }
}

PF_TEST_CASE("allocation failure is reported", "[containers][containers::LLQueue]") {
  using FailQueue = LLQueue<std::uint32_t, ExhaustedResource>;

  SECTION("try_push reports failure without throwing") {
    AllocStats stats;
    FailQueue queue{ExhaustedResource(stats)};

    REQUIRE(!queue.try_push(1));
    REQUIRE(!queue.try_emplace(2));
    REQUIRE(queue.empty());
    REQUIRE(stats.allocations == 2);
  }

  SECTION("push throws") {
    AllocStats stats;
    FailQueue queue{ExhaustedResource(stats)};

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
    REQUIRE_THROWS_AS(queue.push(1), FailQueue::AllocError);
    REQUIRE_THROWS_AS(queue.emplace(2), FailQueue::AllocError);
#endif

    REQUIRE(queue.empty());
    REQUIRE(stats.allocations == 2);
  }
}

PF_TEST_CASE("works with a std memory resource adapter",
             "[containers][containers::LLQueue]") {
  using PmrAdapter =
      mem::std_memory_resource_adapter<std::pmr::unsynchronized_pool_resource>;
  using PmrQueue = LLQueue<std::uint32_t, PmrAdapter>;

  std::pmr::unsynchronized_pool_resource pool;
  PmrAdapter adapter(&pool, alignof(PmrQueue::storage_type));
  PmrQueue queue(adapter);

  for (std::uint32_t i = 0; i < 16; ++i) {
    queue.emplace(i);
  }

  REQUIRE(queue.size() == 16);
  for (std::uint32_t i = 0; i < 16; ++i) {
    REQUIRE(queue.pop() == i);
  }
  REQUIRE(queue.empty());
}

PF_TEST_CASE("works with a resource reference", "[containers][containers::LLQueue]") {
  using RefQueue = LLQueue<std::uint32_t, mem::ResourceRef<CountingResource>>;

  static_assert(mem::Resource_c<mem::ResourceRef<CountingResource>>);
  // a reference mirrors the noexcept-ness of the resource it refers to
  static_assert(mem::is_noexcept_resource<mem::ResourceRef<CountingResource>>::value);
  static_assert(!mem::is_noexcept_resource<mem::ResourceRef<ExhaustedResource>>::value);

  SECTION("nodes go through the referenced resource") {
    AllocStats stats;
    CountingResource resource(stats, 3);
    RefQueue queue{mem::ResourceRef<CountingResource>(resource)};

    queue.emplace(1);
    queue.emplace(2);

    // the referenced resource, not the reference, owns the blocks
    REQUIRE(stats.live_blocks == 2);
    REQUIRE(queue.pop() == 1);
    REQUIRE(stats.live_blocks == 1);
    REQUIRE(queue.pop_unchecked() == 2);
    REQUIRE(stats.live_blocks == 0);

    REQUIRE(&queue.resource().get() == &resource);
  }

  SECTION("a null resource is rejected") {
    CountingResource null_resource;

    auto makeQueueFromNull = [&] { IntQueue queue{null_resource}; };
    auto makeQueueFromNullRef = [&] {
      RefQueue queue{mem::ResourceRef<CountingResource>(null_resource)};
    };

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
    REQUIRE_PF_REQUIRE_FAIL(makeQueueFromNull());
    REQUIRE_PF_REQUIRE_FAIL(makeQueueFromNullRef());
#else
    static_cast<void>(makeQueueFromNull);
    static_cast<void>(makeQueueFromNullRef);
#endif
  }

  SECTION("a resource that cannot report nullness is referenced fine") {
    AllocStats stats;
    ExhaustedResource resource(stats);
    LLQueue<std::uint32_t, mem::ResourceRef<ExhaustedResource>> queue{
        mem::ResourceRef<ExhaustedResource>(resource)};

    REQUIRE(!queue.try_push(1));
    REQUIRE(stats.allocations == 1);
  }
}

PF_TEST_CASE("empty error handling", "[containers][containers::LLQueue]") {
  AllocStats stats;
  IntQueue queue{CountingResource(stats)};

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  REQUIRE_THROWS_AS(queue.pop(), IntQueue::EmptyError);
  REQUIRE_THROWS_AS(queue.front(), RequireFail);
  REQUIRE_THROWS_AS(queue.back(), RequireFail);
#endif
  REQUIRE(!queue.try_pop().has_value());
}

} // namespace pf::containers
