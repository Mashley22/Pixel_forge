#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory_resource>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <PixelForgeValidationHelpers/helpers.hpp>
#include <catch2/catch_test_macros.hpp>

import PixelForge.containers;
import PixelForge.core;
import PixelForge.validation_helpers;

namespace pf::adapters {

namespace {

using Node = LLQueue<std::uint32_t>::Node;

/** storage for a node, the queue itself never owns any */
class NodeStorage {
public:
  using Node = LLQueue<std::uint32_t>::storage_type;

  NodeStorage() {
    m_buffer.data =
        pointer_cast<std::byte*>(std::aligned_alloc(alignof(Node), sizeof(Node)));
    m_buffer.size = sizeof(Node);
    std::memset(m_buffer.data, 0, m_buffer.size);
  }

  ~NodeStorage() { std::free(m_buffer.data); }

  NodeStorage(const NodeStorage&) = delete;
  NodeStorage&
  operator=(const NodeStorage&) = delete;

  [[nodiscard]] ObjectStorage<Node>
  storage() {
    return Buffer::from(m_buffer.data, m_buffer.size).as_objects<Node>(1);
  }

  [[nodiscard]] Node*
  data() {
    return pointer_cast<Node*>(m_buffer.data);
  }

private:
  Buffer m_buffer{};
};

template <typename Queue>
concept has_size = requires(Queue& queue) { queue.size(); };

template <typename Queue>
concept has_is_null = requires(Queue& queue) { queue.is_null(); };

template <typename Queue>
concept has_pop_spare = requires(Queue& queue) { queue.pop_spare(); };

} // namespace

PF_TEST_CASE("a fresh adapter is empty and needs no storage", "[containers][LLQueue]") {
  using Queue = LLQueue<std::uint32_t>;

  Queue queue;

  REQUIRE(queue.empty());
  REQUIRE(!queue.try_pop().has_value());
  REQUIRE(queue.empty()); // a failed pop leaves it as it was

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  REQUIRE_THROWS_AS(queue.pop(), Queue::EmptyError);
  REQUIRE_THROWS_AS(queue.front(), RequireFail);
  REQUIRE_THROWS_AS(queue.back(), RequireFail);
#endif
}

PF_TEST_CASE("the caller owns the storage of every node", "[containers][LLQueue]") {
  using Queue = LLQueue<std::uint32_t>;

  std::vector<NodeStorage> storage(4);
  Queue queue;

  for (std::uint32_t i = 0; i < 3; ++i) {
    queue.emplace(storage[static_cast<std::size_t>(i)].storage(), i);
    REQUIRE_FALSE(queue.empty());
  }

  REQUIRE(queue.front() == 0);
  REQUIRE(queue.back() == 2);

  // pop hands back the very node it was at, value still in it
  auto first = queue.pop();
  REQUIRE(first->val == 0);
  REQUIRE(first.get() == storage[0].data());

  auto second = queue.try_pop();
  REQUIRE(second.has_value());
  REQUIRE((*second)->val == 1);

  REQUIRE(queue.pop_unchecked()->val == 2);
  REQUIRE(queue.empty());
}

PF_TEST_CASE("a node handed out by pop is storage emplace can use again",
             "[containers][LLQueue]") {
  using Queue = LLQueue<std::uint32_t>;

  std::vector<NodeStorage> storage(2);
  Queue queue;
  queue.emplace(storage[0].storage(), 1);
  queue.emplace(storage[1].storage(), 2);

  auto* const recycled = queue.pop().get();
  std::destroy_at(&recycled->val);
  queue.emplace(
      Buffer::from(pointer_cast<std::byte*>(recycled), sizeof(Node)).as_objects<Node>(1),
      3);

  REQUIRE(queue.front() == 2);
  REQUIRE(queue.back() == 3);
  REQUIRE(queue.pop_unchecked()->val == 2);
  REQUIRE(queue.pop_unchecked()->val == 3);
  REQUIRE(queue.empty());
}

PF_TEST_CASE("the adapter moves its chain and leaves nothing behind",
             "[containers][LLQueue]") {
  using Queue = LLQueue<std::uint32_t>;

  std::vector<NodeStorage> storage(2);
  Queue source;
  source.emplace(storage[0].storage(), 1);
  source.emplace(storage[1].storage(), 2);

  Queue destination{std::move(source)};
  REQUIRE(source.empty());
  REQUIRE_FALSE(destination.empty());
  REQUIRE(destination.pop_unchecked()->val == 1);
  REQUIRE(destination.pop_unchecked()->val == 2);
  REQUIRE(destination.empty());

  Queue other;
  other.emplace(storage[0].storage(), 7);
  destination = std::move(other);

  REQUIRE(other.empty());
  REQUIRE(destination.pop_unchecked()->val == 7);
  REQUIRE(destination.empty());

  // self move assignment is a no-op
  NodeStorage third;
  destination.emplace(third.storage(), 8);
  auto& alias = destination;
  destination = std::move(alias);
  REQUIRE(destination.pop_unchecked()->val == 8);
  REQUIRE(destination.empty());
}

PF_TEST_CASE("the adapter has no size and no dummy node", "[containers][LLQueue]") {
  using Queue = LLQueue<std::uint32_t>;
  static_assert(!has_size<Queue>);
  static_assert(!has_is_null<Queue>);
  static_assert(!has_pop_spare<Queue>);
  static_assert(!std::is_copy_constructible_v<Queue>);

  std::vector<NodeStorage> storage(2);
  Queue queue;
  queue.emplace(storage[0].storage(), 1);

  // one value, one node: the front node holds the value itself
  REQUIRE(storage[0].data()->val == 1);
  REQUIRE(queue.pop_unchecked() == storage[0].data());
}

} // namespace pf::adapters

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

/**
 *@brief a resource owning its own stats, so that it is default constructible
 *  and a queue built without a resource can still be exercised
 */
class DefaultResource {
public:
  static constexpr bool is_noexcept = true;

  DefaultResource() noexcept = default;

  [[nodiscard]] Buffer
  allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) {
    m_stats.allocations++;
    void* const ptr = std::aligned_alloc(alignment, size);
    if (ptr == nullptr) {
      return Buffer::null();
    }
    m_stats.live_blocks++;
    return Buffer::from(pointer_cast<std::byte*>(ptr), size);
  }

  void
  deallocate(const Buffer& buffer) {
    m_stats.deallocations++;
    m_stats.live_blocks--;
    std::free(buffer.data);
  }

  [[nodiscard]] AllocStats&
  stats() noexcept {
    return m_stats;
  }

private:
  AllocStats m_stats;
};

/**
 *@brief a resource over a std pmr resource, allocation and deallocation
 *  happen at the alignment the resource was built with
 */
class PmrResource {
public:
  PmrResource(std::pmr::memory_resource& resource, std::size_t alignment) noexcept
    : m_resource(&resource), m_alignment(alignment) {}

  [[nodiscard]] bool
  is_null() const noexcept {
    return m_resource == nullptr;
  }

  [[nodiscard]] Buffer
  allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) {
    static_cast<void>(alignment);
    return Buffer{.data =
                      pointer_cast<std::byte*>(m_resource->allocate(size, m_alignment)),
                  .size = size};
  }

  void
  deallocate(const Buffer& buffer) {
    m_resource->deallocate(pointer_cast<void*>(buffer.data), buffer.size, m_alignment);
  }

  [[nodiscard]] bool
  is_interoperable(const PmrResource& other) const noexcept {
    return m_resource == other.m_resource && m_alignment == other.m_alignment;
  }

private:
  std::pmr::memory_resource* m_resource;
  std::size_t m_alignment;
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

/**
 *@brief a resource that reports itself interoperable with every resource
 *  sharing its token, so that a move between two of them can be trivial
 *
 *@note the tag is what makes two of these distinct types, which is what the
 *  cross resource operations are for
 */
template <typename Tag>
class TaggedResource {
public:
  static constexpr bool is_noexcept = true;

  TaggedResource(AllocStats& stats, std::size_t token) noexcept
    : m_stats(&stats), m_token(token) {}

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

  [[nodiscard]] std::size_t
  token() const noexcept {
    return m_token;
  }

  /** resources of different tags interoperate when they share a token */
  template <typename OtherTag>
  [[nodiscard]] bool
  is_interoperable(const TaggedResource<OtherTag>& other) const noexcept {
    return m_token == other.token();
  }

private:
  AllocStats* m_stats;
  std::size_t m_token;
};

struct FirstTag {};
struct SecondTag {};

using FirstResource = TaggedResource<FirstTag>;
using SecondResource = TaggedResource<SecondTag>;

using IntQueue = LLQueue<std::uint32_t, CountingResource>;
using MoveOnlyQueue = LLQueue<MoveOnlyValue, CountingResource>;
using RefQueue = LLQueue<std::uint32_t, mem::ResourceRef<CountingResource>>;
using MoveOnlyRefQueue = LLQueue<MoveOnlyValue, mem::ResourceRef<CountingResource>>;
using FirstQueue = LLQueue<std::uint32_t, FirstResource>;
using SecondQueue = LLQueue<std::uint32_t, SecondResource>;

} // namespace

static_assert(mem::Resource_c<CountingResource>);
static_assert(mem::Resource_c<ExhaustedResource>);
static_assert(mem::Resource_c<PmrResource>);
static_assert(!mem::is_noexcept_resource<ExhaustedResource>::value);
static_assert(mem::is_noexcept_resource<CountingResource>::value);
static_assert(mem::is_noexcept_resource<FirstResource>::value);
static_assert(mem::is_noexcept_resource<DefaultResource>::value);
static_assert(!std::is_same_v<FirstResource, SecondResource>);

PF_TEST_CASE("construction and type traits", "[containers][LLQueue]") {
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
    static_assert(std::is_default_constructible_v<Queue>);
    static_assert(std::is_copy_constructible_v<Queue>);
    static_assert(std::is_copy_assignable_v<Queue>);
    static_assert(std::is_move_constructible_v<Queue>);
    static_assert(std::is_move_assignable_v<Queue>);
  }

  SECTION("a fresh queue is empty and has allocated nothing") {
    AllocStats stats;
    IntQueue queue{CountingResource(stats)};

    REQUIRE(queue.empty());
    REQUIRE(queue.empty());
    REQUIRE(stats.live_blocks == 0);
    REQUIRE(!queue.try_pop().has_value());
  }

  SECTION("a default constructed queue is empty, but has no resource to use") {
    IntQueue queue;

    REQUIRE(queue.empty());
    REQUIRE(queue.empty());

    auto useIt = [&] { queue.emplace(1); };

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
    REQUIRE_PF_REQUIRE_FAIL(useIt());
#else
    static_cast<void>(useIt);
#endif
  }

  SECTION("a default constructed queue can be given a resource by move assignment") {
    AllocStats stats;

    IntQueue queue;
    {
      IntQueue owned{CountingResource(stats)};
      owned.emplace(7);

      queue = std::move(owned);
    }

    REQUIRE(queue.resource().id() == 0);
    queue.emplace(8);

    REQUIRE(stats.live_blocks == (2));
    REQUIRE(queue.pop() == 7);
    REQUIRE(queue.pop() == 8);
    REQUIRE(stats.live_blocks == 0);
  }

  SECTION("the resource is held by value") {
    AllocStats stats;
    CountingResource resource(stats, 7);
    IntQueue queue(resource);

    queue.emplace(42);

    // the queue allocates through its own copy of the resource
    REQUIRE(stats.live_blocks == (1));
    REQUIRE(stats.allocations == (1));
    REQUIRE(queue.resource().id() == 7);

    REQUIRE(queue.pop() == 42);
    REQUIRE(stats.live_blocks == 0);
  }

  SECTION("a resource can be handed over as an rvalue") {
    AllocStats stats;
    IntQueue queue{CountingResource(stats)};

    queue.emplace(1);
    REQUIRE(stats.live_blocks == (1));
    REQUIRE(queue.pop_unchecked() == 1);
    REQUIRE(stats.live_blocks == 0);
  }
}

PF_TEST_CASE("FIFO order and automatic node reclamation", "[containers][LLQueue]") {
  AllocStats stats;
  IntQueue queue{CountingResource(stats)};

  constexpr std::size_t count = 8;
  for (std::size_t i = 0; i < count; ++i) {
    queue.emplace(static_cast<std::uint32_t>(i + 10));
    REQUIRE(!queue.empty());
    REQUIRE(stats.live_blocks == (i + 1));
  }

  REQUIRE(queue.front() == 10);
  REQUIRE(queue.back() == 17);
  REQUIRE(stats.allocations == (count));

  for (std::size_t i = 0; i < count; ++i) {
    const auto expected = static_cast<std::uint32_t>(i + 10);

    if (i % 2 == 0) {
      auto value = queue.try_pop();
      REQUIRE(value.has_value());
      REQUIRE(*value == expected);
    } else {
      REQUIRE(queue.pop() == expected);
    }

    // each pop hands its node back to the resource
    REQUIRE(stats.live_blocks == count - i - 1);
    REQUIRE(queue.empty() == (i + 1 == count));
  }

  REQUIRE(queue.empty());
  REQUIRE(!queue.try_pop().has_value());
  REQUIRE(stats.deallocations == count);
  REQUIRE(stats.live_blocks == 0);
}

PF_TEST_CASE("destructor returns every node", "[containers][LLQueue]") {
  AllocStats stats;

  {
    IntQueue queue{CountingResource(stats)};
    queue.emplace(1);
    queue.emplace(2);
    queue.emplace(3);
    REQUIRE(stats.live_blocks == (3));
  }

  REQUIRE(stats.live_blocks == 0);
  REQUIRE(stats.deallocations == (3));
}

PF_TEST_CASE("clear returns every node", "[containers][LLQueue]") {
  AllocStats stats;
  IntQueue queue{CountingResource(stats)};

  queue.emplace(1);
  queue.emplace(2);
  REQUIRE(stats.live_blocks == (2));

  queue.clear();
  REQUIRE(queue.empty());
  REQUIRE(queue.empty());
  REQUIRE(stats.live_blocks == 0);

  // the queue stays usable after a clear
  queue.emplace(3);
  REQUIRE_FALSE(queue.empty());
  REQUIRE(queue.pop_unchecked() == 3);
  REQUIRE(stats.live_blocks == 0);
}

PF_TEST_CASE("push and emplace overloads", "[containers][LLQueue]") {
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
    REQUIRE_FALSE(queue.empty());
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

PF_TEST_CASE("allocation failure is reported", "[containers][LLQueue]") {
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

PF_TEST_CASE("works over a std pmr resource", "[containers][LLQueue]") {
  using PmrQueue = LLQueue<std::uint32_t, PmrResource>;

  std::pmr::unsynchronized_pool_resource pool;
  PmrQueue queue{PmrResource(pool, alignof(PmrQueue::storage_type))};

  for (std::uint32_t i = 0; i < 16; ++i) {
    queue.emplace(i);
  }

  REQUIRE_FALSE(queue.empty());
  REQUIRE(queue.front() == 0);
  REQUIRE(queue.back() == 15);
  for (std::uint32_t i = 0; i < 16; ++i) {
    REQUIRE(queue.pop() == i);
  }
  REQUIRE(queue.empty());

  // reusable afterwards, and copyable onto another resource
  queue.emplace(1);
  PmrQueue other{queue, PmrResource(pool, alignof(PmrQueue::storage_type))};
  REQUIRE(other.pop() == 1);
  REQUIRE(queue.pop() == 1);
}

PF_TEST_CASE("works with a resource reference", "[containers][LLQueue]") {
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
    REQUIRE(stats.live_blocks == (2));
    REQUIRE(queue.pop() == 1);
    REQUIRE(stats.live_blocks == (1));
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

PF_TEST_CASE("move semantics", "[containers][LLQueue]") {
  SECTION("move construction hands the nodes over untouched") {
    AllocStats stats;

    {
      IntQueue source{CountingResource(stats)};
      source.emplace(1);
      source.emplace(2);

      IntQueue destination{std::move(source)};
      REQUIRE_FALSE(destination.empty());
      REQUIRE(stats.live_blocks == (2));
      REQUIRE(source.empty());

      REQUIRE(destination.pop() == 1);
      REQUIRE(destination.pop() == 2);
    }

    REQUIRE(stats.live_blocks == 0);
  }

  SECTION("move assignment frees the destination's nodes to its own resource first") {
    AllocStats destination_stats;
    AllocStats source_stats;
    IntQueue destination{CountingResource(destination_stats, 1)};
    IntQueue source{CountingResource(source_stats, 2)};

    destination.emplace(10);
    destination.emplace(20);
    source.emplace(30);
    REQUIRE(destination_stats.live_blocks == (2));
    REQUIRE(source_stats.live_blocks == (1));

    destination = std::move(source);

    // the destination's own nodes went back to the resource that made them
    REQUIRE(destination_stats.live_blocks == 0);
    REQUIRE(destination_stats.deallocations == (2));
    // and the stolen node now travels with the source's resource
    REQUIRE(destination.resource().id() == 2);
    REQUIRE(source_stats.live_blocks == (1));
    REQUIRE(source.empty());

    REQUIRE(destination.pop() == 30);
    REQUIRE(source_stats.live_blocks == 0);
  }

  SECTION("moving an empty queue over a full one") {
    AllocStats stats;
    IntQueue source{CountingResource(stats)};
    IntQueue destination{CountingResource(stats)};

    source.emplace(1);
    destination.emplace(2);
    destination.emplace(3);

    destination = std::move(source);

    REQUIRE_FALSE(destination.empty());
    REQUIRE(destination.pop() == 1);
    REQUIRE(stats.live_blocks == 0);
  }

  SECTION("self move assignment is a no-op") {
    AllocStats stats;
    IntQueue queue{CountingResource(stats, 5)};
    queue.emplace(1);

    auto& alias = queue;
    queue = std::move(alias);

    REQUIRE_FALSE(queue.empty());
    REQUIRE(queue.resource().id() == 5);
    REQUIRE(queue.pop() == 1);
    REQUIRE(stats.live_blocks == 0);
  }
}

PF_TEST_CASE("copying across resource types", "[containers][LLQueue]") {
  // copyable from a queue with another resource type as well as from its own
  static_assert(std::is_constructible_v<IntQueue, RefQueue>);
  static_assert(std::is_constructible_v<IntQueue, RefQueue, CountingResource>);
  static_assert(std::is_constructible_v<IntQueue, IntQueue>);
  static_assert(std::is_assignable_v<IntQueue&, RefQueue>);
  static_assert(std::is_assignable_v<IntQueue&, IntQueue>);

  SECTION("a copy is independent, each side frees through its own resource") {
    AllocStats source_stats;
    AllocStats copy_stats;
    CountingResource source_resource(source_stats);
    CountingResource copy_resource(copy_stats, 9);

    {
      RefQueue source{mem::ResourceRef<CountingResource>(source_resource)};
      source.emplace(1);
      source.emplace(2);

      IntQueue copy{source, copy_resource};

      REQUIRE_FALSE(copy.empty());
      REQUIRE(copy.resource().id() == 9);
      REQUIRE(copy_stats.live_blocks == (2));
      REQUIRE(source_stats.live_blocks == (2));

      // popping from the copy leaves the source alone
      REQUIRE(copy.pop() == 1);
      REQUIRE(copy_stats.live_blocks == (1));
      REQUIRE_FALSE(source.empty());

      REQUIRE(copy.pop_unchecked() == 2);
      REQUIRE(source.pop() == 1);
      REQUIRE(source.pop() == 2);
    }

    REQUIRE(copy_stats.live_blocks == 0);
    REQUIRE(source_stats.live_blocks == 0);
  }

  SECTION("a move across resource types moves values and leaves the source's nodes") {
    AllocStats source_stats;
    AllocStats moved_stats;
    CountingResource source_resource(source_stats);
    CountingResource moved_resource(moved_stats, 4);

    {
      MoveOnlyRefQueue source{mem::ResourceRef<CountingResource>(source_resource)};
      source.emplace(MoveOnlyValue{7});

      // move only values prove nothing is copied
      MoveOnlyQueue moved{std::move(source), moved_resource};

      REQUIRE_FALSE(source.empty()); // the source keeps its nodes
      REQUIRE(moved.pop().value == 7);
      REQUIRE(moved_stats.live_blocks == 0);
    }

    REQUIRE(source_stats.live_blocks == 0);
    REQUIRE(moved_stats.live_blocks == 0);
  }

  SECTION("copy assignment frees the destination through its own resource") {
    AllocStats source_stats;
    AllocStats destination_stats;
    CountingResource source_resource(source_stats);
    CountingResource destination_resource(destination_stats, 1);

    RefQueue source{mem::ResourceRef<CountingResource>(source_resource)};
    source.emplace(5);

    IntQueue destination{destination_resource};
    destination.emplace(99);
    REQUIRE(destination_stats.live_blocks == (1));

    destination = source;

    // 99 went back to the destination's resource, 5 was taken from it too
    REQUIRE(destination_stats.deallocations == (1));
    REQUIRE(destination_stats.live_blocks == (1));
    REQUIRE(destination_stats.allocations == 2);
    REQUIRE_FALSE(destination.empty());
    REQUIRE(destination.resource().id() == 1);
    REQUIRE(destination.pop() == 5);
    REQUIRE(destination_stats.live_blocks == 0);
    REQUIRE_FALSE(source.empty());
  }

  SECTION("move assignment across resource types") {
    AllocStats source_stats;
    AllocStats destination_stats;
    CountingResource source_resource(source_stats);
    CountingResource destination_resource(destination_stats, 1);

    MoveOnlyRefQueue source{mem::ResourceRef<CountingResource>(source_resource)};
    source.emplace(MoveOnlyValue{11});

    MoveOnlyQueue destination{destination_resource};
    destination = std::move(source);

    REQUIRE_FALSE(destination.empty());
    REQUIRE(destination.pop().value == 11);
    REQUIRE(destination_stats.live_blocks == 0);
    REQUIRE_FALSE(source.empty());
    REQUIRE(source.pop().value == 11);
    REQUIRE(source_stats.live_blocks == 0);
  }

  SECTION("a destination without a resource is rejected") {
    AllocStats source_stats;
    CountingResource source_resource(source_stats);
    RefQueue source{mem::ResourceRef<CountingResource>(source_resource)};
    source.emplace(1);

    IntQueue destination;
    auto assignToIt = [&] { destination = source; };

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
    REQUIRE_PF_REQUIRE_FAIL(assignToIt());
#else
    static_cast<void>(assignToIt);
#endif
  }
}

PF_TEST_CASE("moving across resource types", "[containers][LLQueue]") {
  // the two tagged resources are distinct types reporting interoperability
  static_assert(!std::is_same_v<FirstResource, SecondResource>);
  static_assert(std::is_constructible_v<SecondQueue, FirstQueue&&, SecondResource>);
  static_assert(std::is_assignable_v<SecondQueue&, FirstQueue&&>);

  SECTION("interoperable resources take the node chain over as is") {
    AllocStats source_stats;
    AllocStats destination_stats;
    FirstResource source_resource(source_stats, 1);
    SecondResource destination_resource(destination_stats, 1);

    FirstQueue source{source_resource};
    source.emplace(1);
    REQUIRE(source_stats.allocations == (1));

    SecondQueue destination{std::move(source), destination_resource};

    // nothing was allocated for the destination, the chain was taken over
    REQUIRE(destination_stats.allocations == 0);
    REQUIRE_FALSE(destination.empty());
    REQUIRE(source.empty());

    // and the stolen node is freed through the destination's resource
    REQUIRE(destination.pop() == 1);
    REQUIRE(destination_stats.deallocations == (1));
    REQUIRE(source_stats.live_blocks == 1);
  }

  SECTION("resources that are not interoperable rebuild the nodes") {
    AllocStats source_stats;
    AllocStats destination_stats;
    FirstResource source_resource(source_stats, 1);
    SecondResource destination_resource(destination_stats, 2);

    FirstQueue source{source_resource};
    source.emplace(1);

    SecondQueue destination{std::move(source), destination_resource};

    // the value was moved into a node of the destination's resource
    REQUIRE(destination_stats.allocations == (1));
    REQUIRE(destination_stats.live_blocks == (1));
    REQUIRE_FALSE(destination.empty());
    // and the source keeps its own nodes
    REQUIRE_FALSE(source.empty());
    REQUIRE(source_stats.live_blocks == (1));

    REQUIRE(destination.pop() == 1);
    REQUIRE(destination_stats.deallocations == (1));
    REQUIRE(source.pop() == 1);
    REQUIRE(source_stats.deallocations == 1);
  }

  SECTION("interoperable move assignment also steals the chain") {
    AllocStats source_stats;
    AllocStats destination_stats;
    FirstResource source_resource(source_stats, 1);
    SecondResource destination_resource(destination_stats, 1);

    SecondQueue destination{destination_resource};
    destination.emplace(99);
    REQUIRE(destination_stats.live_blocks == (1));

    FirstQueue source{source_resource};
    source.emplace(1);
    source.emplace(2);

    destination = std::move(source);

    // 99 went back to the destination's resource, then no allocation happened
    REQUIRE(destination_stats.deallocations == (1));
    REQUIRE(destination_stats.allocations == (1));
    REQUIRE_FALSE(destination.empty());

    REQUIRE(destination.pop() == 1);
    REQUIRE(destination.pop_unchecked() == 2);
    // the two stolen nodes go back through the destination's resource
    REQUIRE(destination_stats.deallocations == 3);
    REQUIRE(source.empty());
    REQUIRE(source_stats.live_blocks == (2));
  }

  SECTION("move assignment between resources that are not interoperable rebuilds") {
    AllocStats source_stats;
    AllocStats destination_stats;
    FirstResource source_resource(source_stats, 1);
    SecondResource destination_resource(destination_stats, 2);

    SecondQueue destination{destination_resource};
    FirstQueue source{source_resource};
    source.emplace(1);

    destination = std::move(source);

    REQUIRE(destination_stats.allocations == (1));
    REQUIRE(destination_stats.live_blocks == (1));
    REQUIRE(destination.pop() == 1);
    REQUIRE(destination_stats.deallocations == (1));
    // the source still owns its node
    REQUIRE_FALSE(source.empty());
    REQUIRE(source.pop() == 1);
    REQUIRE(source_stats.deallocations == 1);
  }

  SECTION("a resource that cannot report interoperability takes the slow path") {
    AllocStats source_stats;
    AllocStats destination_stats;
    CountingResource source_resource(source_stats);
    CountingResource destination_resource(destination_stats, 3);

    IntQueue source{source_resource};
    source.emplace(1);

    IntQueue destination{std::move(source), destination_resource};

    REQUIRE(destination_stats.allocations == (1));
    REQUIRE_FALSE(destination.empty());
    REQUIRE(destination.pop() == 1);
    REQUIRE_FALSE(source.empty());
  }
}

PF_TEST_CASE("copying with a default constructed resource", "[containers][LLQueue]") {
  using DefaultQueue = LLQueue<std::uint32_t, DefaultResource>;

  SECTION("copy construction from a queue of the same type") {
    DefaultQueue source;
    source.emplace(1);
    source.emplace(2);
    REQUIRE(source.resource().stats().live_blocks == (2));

    DefaultQueue copy{source};

    // the copy got a default constructed resource of its own
    REQUIRE_FALSE(copy.empty());
    REQUIRE(copy.resource().stats().live_blocks == (2));
    REQUIRE(source.resource().stats().live_blocks == (2));

    REQUIRE(copy.pop() == 1);
    REQUIRE(copy.resource().stats().live_blocks == (1));
    REQUIRE(copy.pop_unchecked() == 2);
    REQUIRE(copy.resource().stats().live_blocks == 0);
    REQUIRE(source.resource().stats().live_blocks == (2));
  }

  SECTION("copy assignment between queues of the same type") {
    DefaultQueue source;
    source.emplace(1);
    source.emplace(2);

    DefaultQueue destination;
    destination.emplace(99);
    REQUIRE(destination.resource().stats().live_blocks == (1));

    destination = source;

    REQUIRE_FALSE(destination.empty());
    REQUIRE(destination.resource().stats().live_blocks == (2));
    REQUIRE(destination.resource().stats().deallocations == (1));
    REQUIRE(destination.pop() == 1);
    REQUIRE_FALSE(source.empty());
  }

  SECTION("self copy assignment is a no-op") {
    DefaultQueue queue;
    queue.emplace(1);

    auto& alias = queue;
    queue = alias;

    REQUIRE_FALSE(queue.empty());
    REQUIRE(queue.pop() == 1);
  }

  SECTION("cross resource copy and move with a default constructed resource") {
    AllocStats stats;
    CountingResource resource(stats);

    RefQueue to_copy{mem::ResourceRef<CountingResource>(resource)};
    to_copy.emplace(1);

    DefaultQueue copied{to_copy};
    REQUIRE_FALSE(copied.empty());
    REQUIRE(copied.resource().stats().live_blocks == (1));
    REQUIRE(copied.pop() == 1);
    REQUIRE_FALSE(to_copy.empty());

    RefQueue to_move{mem::ResourceRef<CountingResource>(resource)};
    to_move.emplace(2);

    DefaultQueue moved{std::move(to_move)};
    REQUIRE_FALSE(moved.empty());
    REQUIRE(moved.pop_unchecked() == 2);
    // a default constructed resource cannot report interoperability, so the
    // values were rebuilt rather than the chain taken over
    REQUIRE_FALSE(to_move.empty());
  }

  SECTION("cross resource copy and move assignment with a default resource") {
    AllocStats stats;
    CountingResource resource(stats);
    DefaultQueue destination;

    RefQueue source{mem::ResourceRef<CountingResource>(resource)};
    source.emplace(5);

    destination = source;
    REQUIRE_FALSE(destination.empty());
    REQUIRE(destination.pop() == 5);
    REQUIRE_FALSE(source.empty());

    destination = std::move(source);
    REQUIRE_FALSE(destination.empty());
    REQUIRE(destination.pop_unchecked() == 5);
    REQUIRE_FALSE(source.empty());
    REQUIRE(destination.resource().stats().live_blocks == 0);
  }
}

PF_TEST_CASE("empty error handling", "[containers][LLQueue]") {
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
