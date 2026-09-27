#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <PixelForgeValidationHelpers/helpers.hpp>
#include <catch2/catch_test_macros.hpp>

import PixelForge.adapters;
import PixelForge.core;
import PixelForge.validation_helpers;

namespace pf::adapters {

namespace {

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

template <typename T>
class NodeStorage {
public:
  using Node = SPSCLLQueue<T>::storage_type;

  [[nodiscard]] ObjectStorage<Node>
  objStore() {
    return Buffer::from(buffer.data, buffer.size).template asObjects<Node>(1);
  }

  [[nodiscard]] Node*
  data() {
    return pointer_cast<Node*>(buffer.data);
  }

  NodeStorage() {
    buffer.size = sizeof(Node);
    buffer.data =
        pointer_cast<std::byte*>(std::aligned_alloc(alignof(Node), sizeof(Node)));
    std::memset(buffer.data, 0, buffer.size);
  }

  ~NodeStorage() { std::free(buffer.data); }

  // The queue's dummy node is intentionally represented by raw storage.
  Buffer buffer{};
};

} // namespace

PF_TEST_CASE("construction and type traits", "[adapters][SPSCLLQueue]") {
  using Queue = SPSCLLQueue<std::uint32_t>;
  using Node = Queue::storage_type;

  SECTION("Traits") {
    static_assert(std::is_same_v<Queue::value_type, std::uint32_t>);
    static_assert(std::is_same_v<Queue::size_type, std::size_t>);
    static_assert(std::is_same_v<Queue::difference_type, std::ptrdiff_t>);
    static_assert(std::is_same_v<Queue::reference, std::uint32_t&>);
    static_assert(std::is_same_v<Queue::const_reference, const std::uint32_t&>);
    static_assert(std::is_same_v<Queue::pointer, std::uint32_t*>);
    static_assert(std::is_same_v<Queue::const_pointer, const std::uint32_t*>);
    static_assert(std::is_same_v<Queue::storage_type, Node>);
    static_assert(std::is_same_v<decltype(Node::next), std::atomic<Node*>>);
    static_assert(std::is_same_v<pf::adapters::SPSCLLQueue<std::uint32_t>, Queue>);
    static_assert(std::is_default_constructible_v<Queue>);
    static_assert(!std::is_copy_constructible_v<Queue>);
    static_assert(!std::is_copy_assignable_v<Queue>);
    static_assert(std::is_move_constructible_v<Queue>);
    static_assert(std::is_move_assignable_v<Queue>);
  }

  NodeStorage<std::uint32_t> dummy;
  Queue queue(dummy.objStore());

  REQUIRE(queue.empty());
  REQUIRE(dummy.data()->next.load(std::memory_order_relaxed) == nullptr);
  REQUIRE_FALSE(queue.isNull());

  REQUIRE(queue.popDummy() == dummy.data());
  REQUIRE(queue.isNull());

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  SECTION("invalid dummy storage is rejected") {
    ObjectStorage<Node> emptyStorage{.data = dummy.data(), .size = 0};
    auto makeEmptyStorageQueue = [&] { Queue candidateQueue(emptyStorage); };
    REQUIRE_PF_REQUIRE_FAIL(makeEmptyStorageQueue());

    alignas(Node) std::array<std::byte, 2 * sizeof(Node)> twoNodes{};
    ObjectStorage<Node> oversizedStorage =
        Buffer::from(twoNodes.data(), twoNodes.size()).asObjects<Node>(2);
    auto makeOversizedStorageQueue = [&] { Queue candidateQueue(oversizedStorage); };
    REQUIRE_PF_REQUIRE_FAIL(makeOversizedStorageQueue());
  }
#endif
}

PF_TEST_CASE("move construction", "[adapters][SPSCLLQueue]") {
  using Queue = SPSCLLQueue<std::uint32_t>;

  std::array<NodeStorage<std::uint32_t>, 3> storage;
  Queue source(storage[0].objStore());
  source.emplace(storage[1].objStore(), 10);
  source.emplace(storage[2].objStore(), 20);

  Queue moved(std::move(source));

  // source is moved-from and is intentionally not used before destruction.
  auto* first = moved.pop();
  REQUIRE(first == storage[0].data());
  REQUIRE(first->val == 10);
  std::destroy_at(&first->val);

  auto* second = moved.pop();
  REQUIRE(second == storage[1].data());
  REQUIRE(second->val == 20);
  std::destroy_at(&second->val);

  REQUIRE(moved.empty());
  REQUIRE(moved.popDummy() == storage[2].data());
}

PF_TEST_CASE("move assignment", "[adapters][SPSCLLQueue]") {
  using Queue = SPSCLLQueue<std::uint32_t>;

  SECTION("null destination takes ownership from a non-null source") {
    std::array<NodeStorage<std::uint32_t>, 3> storage;
    Queue source(storage[0].objStore());
    source.emplace(storage[1].objStore(), 10);
    source.emplace(storage[2].objStore(), 20);

    Queue destination;
    destination = std::move(source);

    // source is moved-from and is intentionally not used before destruction.
    auto* first = destination.pop();
    REQUIRE(first == storage[0].data());
    REQUIRE(first->val == 10);
    std::destroy_at(&first->val);

    auto* second = destination.pop();
    REQUIRE(second == storage[1].data());
    REQUIRE(second->val == 20);
    std::destroy_at(&second->val);

    REQUIRE(destination.empty());
    REQUIRE(destination.popDummy() == storage[2].data());
  }
}

PF_TEST_CASE("FIFO order and pop semantics", "[adapters][SPSCLLQueue]") {
  using Queue = SPSCLLQueue<std::uint32_t>;

  constexpr std::size_t count = 8;
  std::array<NodeStorage<std::uint32_t>, count + 1> storage;
  Queue queue(storage[0].objStore());

  for (std::size_t i = 0; i < count; ++i) {
    queue.emplace(storage[i + 1].objStore(), static_cast<std::uint32_t>(i + 10));
    REQUIRE(!queue.empty());
  }

  REQUIRE(storage[0].data()->next.load(std::memory_order_acquire) == storage[1].data());

  for (std::size_t i = 0; i < count; ++i) {
    const auto expected = static_cast<std::uint32_t>(i + 10);
    REQUIRE(!queue.empty());

    auto* node = queue.pop();
    REQUIRE(node == storage[i].data());
    REQUIRE(node->val == expected);
    std::destroy_at(&node->val);
  }

  REQUIRE(queue.empty());
  REQUIRE(queue.pop() == nullptr);
  REQUIRE(queue.popDummy() != nullptr);
}

PF_TEST_CASE("push and emplace overloads", "[adapters][SPSCLLQueue]") {
  SECTION("copy push preserves the source value") {
    using Queue = SPSCLLQueue<std::uint32_t>;

    std::array<NodeStorage<std::uint32_t>, 2> storage;
    Queue queue(storage[0].objStore());

    std::uint32_t source = 17;
    queue.push(storage[1].objStore(), source);
    source = 99;

    auto* node = queue.pop();
    REQUIRE(node == storage[0].data());
    REQUIRE(node->val == 17);
    std::destroy_at(&node->val);
    REQUIRE(queue.popDummy() != nullptr);
  }

  SECTION("rvalue push accepts a moved value") {
    using Queue = SPSCLLQueue<std::uint32_t>;

    std::array<NodeStorage<std::uint32_t>, 2> storage;
    Queue queue(storage[0].objStore());

    std::uint32_t source = 23;
    queue.push(storage[1].objStore(), std::move(source));

    auto* node = queue.pop();
    REQUIRE(node == storage[0].data());
    REQUIRE(node->val == 23);
    std::destroy_at(&node->val);
    REQUIRE(queue.popDummy() != nullptr);
  }

  SECTION("emplace forwards constructor arguments") {
    using Queue = SPSCLLQueue<std::uint32_t>;

    std::array<NodeStorage<std::uint32_t>, 2> storage;
    Queue queue(storage[0].objStore());

    queue.emplace(storage[1].objStore(), 31);

    auto* node = queue.pop();
    REQUIRE(node == storage[0].data());
    REQUIRE(node->val == 31);
    std::destroy_at(&node->val);
    REQUIRE(queue.popDummy() != nullptr);
  }

  SECTION("move-only values can be queued") {
    using Queue = SPSCLLQueue<MoveOnlyValue>;

    std::array<NodeStorage<MoveOnlyValue>, 2> storage;
    Queue queue(storage[0].objStore());

    MoveOnlyValue source{41};
    queue.push(storage[1].objStore(), std::move(source));

    auto* node = queue.pop();
    REQUIRE(node == storage[0].data());
    REQUIRE(node->val.value == 41);
    std::destroy_at(&node->val);
    REQUIRE(queue.popDummy() != nullptr);
  }
}

PF_TEST_CASE("popping an empty queue", "[adapters][SPSCLLQueue]") {
  using Queue = SPSCLLQueue<std::uint32_t>;

  NodeStorage<std::uint32_t> dummy;
  NodeStorage<std::uint32_t> value;
  Queue queue(dummy.objStore());

  REQUIRE(queue.empty());
  REQUIRE(queue.pop() == nullptr);
  REQUIRE(queue.empty());

  queue.emplace(value.objStore(), 42);

  auto* node = queue.pop();
  REQUIRE(node == dummy.data());
  REQUIRE(node->val == 42);
  std::destroy_at(&node->val);
  REQUIRE(queue.empty());
  REQUIRE(queue.popDummy() != nullptr);
}

PF_TEST_CASE("single producer and consumer", "[adapters][SPSCLLQueue]") {
  using Queue = SPSCLLQueue<std::uint32_t>;

  constexpr std::size_t count = 2000;
  std::vector<std::unique_ptr<NodeStorage<std::uint32_t>>> storage;
  storage.reserve(count + 1);
  for (std::size_t i = 0; i < count + 1; ++i) {
    storage.push_back(std::make_unique<NodeStorage<std::uint32_t>>());
  }

  Queue queue(storage[0]->objStore());
  std::atomic<bool> start{false};
  std::vector<std::uint32_t> consumed;
  consumed.reserve(count);

  std::thread producer([&] {
    while (!start.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    for (std::size_t i = 0; i < count; ++i) {
      queue.emplace(storage[i + 1]->objStore(), static_cast<std::uint32_t>(i));
    }
  });

  std::thread consumer([&] {
    while (!start.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    for (std::size_t i = 0; i < count; ++i) {
      auto* node = queue.pop();
      while (node == nullptr) {
        std::this_thread::yield();
        node = queue.pop();
      }

      consumed.push_back(node->val);
      std::destroy_at(&node->val);
    }
  });

  start.store(true, std::memory_order_release);
  producer.join();
  consumer.join();

  REQUIRE(consumed.size() == count);
  for (std::size_t i = 0; i < count; ++i) {
    REQUIRE(consumed[i] == i);
  }
  REQUIRE(queue.empty());
  REQUIRE(queue.popDummy() == storage[count]->data());
}

} // namespace pf::adapters
