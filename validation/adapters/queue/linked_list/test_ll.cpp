#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

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
  using Node = LLQueue<T>::storage_type;

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

  Buffer buffer{};
};

} // namespace

PF_TEST_CASE("construction and type traits", "[adapters][LLQueue]") {
  using Queue = LLQueue<std::uint32_t>;
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
    static_assert(std::is_same_v<decltype(Node::next), Node*>);
    static_assert(std::is_same_v<pf::adapters::LLQueue<std::uint32_t>, Queue>);
    static_assert(std::is_default_constructible_v<Queue>);
    static_assert(!std::is_copy_constructible_v<Queue>);
    static_assert(!std::is_copy_assignable_v<Queue>);
    static_assert(std::is_move_constructible_v<Queue>);
    static_assert(std::is_move_assignable_v<Queue>);
  }

  NodeStorage<std::uint32_t> dummy;
  std::construct_at(dummy.data());
  Queue queue(dummy.objStore());

  REQUIRE(queue.empty());
  REQUIRE(dummy.data()->next == nullptr);

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  SECTION("invalid dummy storage is rejected") {
    ObjectStorage<Node> emptyStorage{.data = dummy.data(), .size = 0};
    auto makeEmptyStorageQueue = [&] { Queue candidateQueue(emptyStorage); };
    REQUIRE_PF_REQUIRE_FAIL(makeEmptyStorageQueue());

    ObjectStorage<Node> nullStorage{.data = nullptr, .size = 1};
    auto makeNullStorageQueue = [&] { Queue candidateQueue(nullStorage); };
    REQUIRE_PF_REQUIRE_FAIL(makeNullStorageQueue());

    alignas(Node) std::array<std::byte, 2 * sizeof(Node)> twoNodes{};
    ObjectStorage<Node> oversizedStorage =
        Buffer::from(twoNodes.data(), twoNodes.size()).asObjects<Node>(2);
    auto makeOversizedStorageQueue = [&] { Queue candidateQueue(oversizedStorage); };
    REQUIRE_PF_REQUIRE_FAIL(makeOversizedStorageQueue());
  }
#endif
}

PF_TEST_CASE("FIFO order and pop policies", "[adapters][LLQueue]") {
  using Queue = LLQueue<std::uint32_t>;
  using Node = Queue::storage_type;

  constexpr std::size_t count = 8;
  std::array<NodeStorage<std::uint32_t>, count + 1> storage;
  std::construct_at(storage[0].data());
  Queue queue(storage[0].objStore());

  for (std::size_t i = 0; i < count; ++i) {
    queue.emplace(storage[i + 1].objStore(), static_cast<std::uint32_t>(i + 10));
    REQUIRE(!queue.empty());
  }

  REQUIRE(storage[0].data()->next == storage[1].data());

  for (std::size_t i = 0; i < count; ++i) {
    const auto expected = static_cast<std::uint32_t>(i + 10);
    REQUIRE(!queue.empty());

    std::optional<NonNull<Node*>> node;
    if (i % 2 == 0) {
      node = queue.try_pop();
      REQUIRE(node.has_value());
    } else {
      node = queue.pop();
    }

    REQUIRE((*node)->val == expected);
    REQUIRE((*node).get() == storage[i].data());
    std::destroy_at((*node).get());
  }

  REQUIRE(queue.empty());
  REQUIRE(!queue.try_pop().has_value());
}

PF_TEST_CASE("push and emplace overloads", "[adapters][LLQueue]") {
  SECTION("copy push preserves the source value") {
    using Queue = LLQueue<std::uint32_t>;

    std::array<NodeStorage<std::uint32_t>, 2> storage;
    Queue queue(storage[0].objStore());

    std::uint32_t source = 17;
    queue.push(storage[1].objStore(), source);
    source = 99;

    auto node = queue.try_pop();
    REQUIRE(node.has_value());
    REQUIRE((*node)->val == 17);
    std::destroy_at((*node).get());
  }

  SECTION("rvalue push accepts a moved value") {
    using Queue = LLQueue<std::uint32_t>;

    std::array<NodeStorage<std::uint32_t>, 2> storage;
    Queue queue(storage[0].objStore());

    std::uint32_t source = 23;
    queue.push(storage[1].objStore(), std::move(source));

    auto node = queue.try_pop();
    REQUIRE(node.has_value());
    REQUIRE((*node)->val == 23);
    std::destroy_at((*node).get());
  }

  SECTION("emplace forwards constructor arguments") {
    using Queue = LLQueue<std::uint32_t>;

    std::array<NodeStorage<std::uint32_t>, 2> storage;
    Queue queue(storage[0].objStore());

    queue.emplace(storage[1].objStore(), 31);

    auto node = queue.try_pop();
    REQUIRE(node.has_value());
    REQUIRE((*node)->val == 31);
    std::destroy_at((*node).get());
  }

  SECTION("move-only values can be queued") {
    using Queue = LLQueue<MoveOnlyValue>;

    std::array<NodeStorage<MoveOnlyValue>, 2> storage;
    Queue queue(storage[0].objStore());

    MoveOnlyValue source{41};
    queue.push(storage[1].objStore(), std::move(source));

    auto node = queue.try_pop();
    REQUIRE(node.has_value());
    REQUIRE((*node)->val.value == 41);
    std::destroy_at((*node).get());
  }
}

PF_TEST_CASE("empty and unchecked error handling", "[adapters][LLQueue]") {
  using Queue = LLQueue<std::uint32_t>;

  NodeStorage<std::uint32_t> dummy;
  NodeStorage<std::uint32_t> value;
  Queue queue(dummy.objStore());

  REQUIRE(!queue.try_pop().has_value());
#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  REQUIRE_THROWS_AS(queue.pop(), Queue::EmptyError);
#endif
#ifndef NDEBUG
  REQUIRE(queue.empty());
#endif

#ifndef NDEBUG
  auto popEmptyUnchecked = [&] { static_cast<void>(queue.pop_unchecked()); };
  REQUIRE_PF_REQUIRE_FAIL(popEmptyUnchecked());
#endif

  queue.emplace(value.objStore(), 42);

  auto node = queue.pop_unchecked();
  REQUIRE(node->val == 42);
  REQUIRE(node.get() == dummy.data());
  REQUIRE(queue.empty());
  std::destroy_at(node.get());
}

} // namespace pf::adapters
