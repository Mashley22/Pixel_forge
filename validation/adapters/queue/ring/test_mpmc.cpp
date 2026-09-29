#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
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

constexpr std::size_t BUF_SIZE = 128;

} // namespace

PF_TEST_CASE("construction and type traits", "[adapters][MPMCRingQueue]") {
  using Queue = MPMCRingQueue<std::uint32_t>;

  SECTION("Traits") {
    static_assert(std::is_same_v<Queue::value_type, std::uint32_t>);
    static_assert(std::is_same_v<Queue::size_type, std::size_t>);
    static_assert(std::is_same_v<Queue::difference_type, std::ptrdiff_t>);
    static_assert(std::is_same_v<Queue::reference, std::uint32_t&>);
    static_assert(std::is_same_v<Queue::const_reference, const std::uint32_t&>);
    static_assert(std::is_same_v<Queue::pointer, std::uint32_t*>);
    static_assert(std::is_same_v<Queue::const_pointer, const std::uint32_t*>);
    static_assert(std::is_same_v<Queue::storage_type, std::uint32_t>);
    static_assert(std::is_same_v<pf::adapters::MPMCRingQueue<std::uint32_t>, Queue>);
    static_assert(!std::is_default_constructible_v<Queue>);
    static_assert(!std::is_copy_constructible_v<Queue>);
    static_assert(!std::is_copy_assignable_v<Queue>);
    static_assert(!std::is_move_constructible_v<Queue>);
    static_assert(!std::is_move_assignable_v<Queue>);
  }

  alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);
  Queue queue(storage);

  REQUIRE(queue.capacity() == BUF_SIZE);
  REQUIRE(queue.size() == 0);
  REQUIRE(queue.remaining() == BUF_SIZE);
  REQUIRE(queue.empty());
  REQUIRE_FALSE(queue.full());

  // the storage holds raw storage, the queue has not constructed anything
  for (const std::byte ele : buf) {
    REQUIRE(ele == std::byte{});
  }

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  SECTION("invalid storage is rejected") {
    auto nullStorage = ObjectStorage<std::uint32_t>{.data = nullptr, .size = BUF_SIZE};
    auto makeNullQueue = [&] { Queue candidateQueue(nullStorage); };
    REQUIRE_PF_REQUIRE_FAIL(makeNullQueue());

    auto emptyStorage = ObjectStorage<std::uint32_t>{.data = storage.data, .size = 0};
    auto makeEmptyQueue = [&] { Queue candidateQueue(emptyStorage); };
    REQUIRE_PF_REQUIRE_FAIL(makeEmptyQueue());

    // the capacity has to be a power of two
    alignas(std::uint32_t) std::array<std::byte, 3 * sizeof(std::uint32_t)> oddBuf{};
    auto oddStorage = Buffer::from(oddBuf).asObjects<std::uint32_t>(3);
    auto makeOddQueue = [&] { Queue candidateQueue(oddStorage); };
    REQUIRE_PF_REQUIRE_FAIL(makeOddQueue());
  }
#endif
}

PF_TEST_CASE("try_push and try_pop", "[adapters][MPMCRingQueue]") {
  alignas(std::uint32_t) std::array<std::byte, 8 * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(8);
  MPMCRingQueue<std::uint32_t> queue(storage);

  REQUIRE_FALSE(queue.try_pop().has_value());

  for (std::uint32_t i = 0; i < 8; ++i) {
    REQUIRE(queue.try_push(i));
  }

  REQUIRE(queue.full());
  REQUIRE(queue.size() == 8);
  REQUIRE(queue.remaining() == 0);
  REQUIRE_FALSE(queue.try_push(99));

  for (std::uint32_t i = 0; i < 8; ++i) {
    const auto popped = queue.try_pop();
    REQUIRE(popped.has_value());
    REQUIRE(*popped == i);
  }

  REQUIRE(queue.empty());
  REQUIRE(queue.size() == 0);
  REQUIRE_FALSE(queue.try_pop().has_value());

  // the ring wraps around
  for (std::uint32_t i = 0; i < 24; ++i) {
    REQUIRE(queue.try_push(i));
    const auto popped = queue.try_pop();
    REQUIRE(popped.has_value());
    REQUIRE(*popped == i);
  }
}

PF_TEST_CASE("wait_push and wait_pop", "[adapters][MPMCRingQueue]") {
  alignas(std::uint32_t) std::array<std::byte, 8 * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(8);
  MPMCRingQueue<std::uint32_t> queue(storage);

  for (std::uint32_t i = 0; i < 8; ++i) {
    REQUIRE(queue.wait_push(i));
  }

  REQUIRE(queue.full());

  // a push blocks until a consumer frees a slot
  std::atomic<bool> pushed{false};
  std::thread producer([&] {
    for (std::uint32_t i = 0; i < 4; ++i) {
      REQUIRE(queue.wait_push(i));
    }
    pushed.store(true, std::memory_order_release);
  });

  REQUIRE_FALSE(pushed.load(std::memory_order_acquire));
  for (std::uint32_t i = 0; i < 12; ++i) {
    const auto popped = queue.wait_pop();
    REQUIRE(popped.has_value());
    REQUIRE(*popped == (i >= 8 ? i - 8 : i));
  }

  producer.join();
  REQUIRE(pushed.load(std::memory_order_acquire));
  REQUIRE(queue.empty());
}

PF_TEST_CASE("move only values", "[adapters][MPMCRingQueue]") {
  alignas(std::unique_ptr<int>) std::array<std::byte, 4 * sizeof(std::unique_ptr<int>)>
      buf{};
  auto storage = Buffer::from(buf).asObjects<std::unique_ptr<int>>(4);
  MPMCRingQueue<std::unique_ptr<int>> queue(storage);

  auto source = std::make_unique<int>(42);
  REQUIRE(queue.try_push(std::move(source)));
  // the slot is built by moving, so the caller no longer holds the value
  REQUIRE_FALSE(source);

  auto popped = queue.try_pop();
  REQUIRE(popped.has_value());
  REQUIRE(*popped != nullptr);
  REQUIRE(**popped == 42);

  // the blocking path takes it too
  REQUIRE(queue.wait_push(std::make_unique<int>(7)));
  auto waited = queue.wait_pop();
  REQUIRE(waited.has_value());
  REQUIRE(**waited == 7);
  REQUIRE(queue.empty());
}

PF_TEST_CASE("multiple producers and consumers with try_", "[adapters][MPMCRingQueue]") {
  constexpr std::size_t producerCount = 4;
  constexpr std::size_t consumerCount = 4;
  constexpr std::size_t perProducer = 2000;
  constexpr std::size_t total = producerCount * perProducer;

  alignas(std::uint32_t) std::array<std::byte, 64 * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(64);
  MPMCRingQueue<std::uint32_t> queue(storage);

  std::atomic<bool> start{false};
  std::vector<std::vector<std::uint32_t>> consumed(consumerCount);
  std::atomic<std::size_t> popped{0};

  std::vector<std::thread> consumers;
  consumers.reserve(consumerCount);
  for (std::size_t c = 0; c < consumerCount; ++c) {
    consumers.emplace_back([&, c] {
      std::vector<std::uint32_t>& mine = consumed[c];
      mine.reserve(total / consumerCount);

      while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }

      // the try_ operations report failure when another thread is midway, so
      // the counter is what the assertion is made against
      while (mine.size() * consumerCount < total) {
        if (auto popped_ = queue.try_pop()) {
          mine.push_back(*popped_);
          popped.fetch_add(1, std::memory_order_release);
        } else {
          std::this_thread::yield();
        }
      }
    });
  }

  std::vector<std::thread> producers;
  producers.reserve(producerCount);
  for (std::size_t p = 0; p < producerCount; ++p) {
    producers.emplace_back([&, p] {
      while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }

      for (std::size_t i = 0; i < perProducer; ++i) {
        while (!queue.try_push(static_cast<std::uint32_t>(p * perProducer + i))) {
          std::this_thread::yield();
        }
      }
    });
  }

  start.store(true, std::memory_order_release);
  for (std::thread& producer : producers) {
    producer.join();
  }
  for (std::thread& consumer : consumers) {
    consumer.join();
  }

  REQUIRE(popped.load(std::memory_order_acquire) == total);

  // the producers race for the slots, so only the set of values is
  // deterministic, not the order they were consumed in
  std::vector<std::uint32_t> all;
  all.reserve(total);
  for (const std::vector<std::uint32_t>& mine : consumed) {
    all.insert(all.end(), mine.begin(), mine.end());
  }

  REQUIRE(all.size() == total);
  std::sort(all.begin(), all.end());
  for (std::size_t i = 0; i < total; ++i) {
    REQUIRE(all[i] == i);
  }
  REQUIRE(queue.empty());
}

PF_TEST_CASE("multiple producers and consumers with wait_", "[adapters][MPMCRingQueue]") {
  constexpr std::size_t producerCount = 4;
  constexpr std::size_t consumerCount = 4;
  constexpr std::size_t perProducer = 2000;
  constexpr std::size_t total = producerCount * perProducer;

  alignas(std::uint32_t) std::array<std::byte, 64 * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(64);
  MPMCRingQueue<std::uint32_t> queue(storage);

  std::atomic<bool> start{false};
  std::atomic<std::size_t> consumedCount{0};
  std::atomic<bool> popFailed{false};
  std::atomic<bool> pushFailed{false};
  std::vector<std::vector<std::uint32_t>> consumed(consumerCount);

  std::vector<std::thread> consumers;
  consumers.reserve(consumerCount);
  for (std::size_t c = 0; c < consumerCount; ++c) {
    consumers.emplace_back([&, c] {
      std::vector<std::uint32_t>& mine = consumed[c];
      mine.reserve(total / consumerCount);

      while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }

      // every consumer takes a fixed share, so all of them finish. The catch
      // assertions cannot be used from here, they are not thread safe, so
      // the outcome is recorded and checked once the threads are joined.
      while (mine.size() < total / consumerCount) {
        auto popped = queue.wait_pop();
        if (!popped) {
          popFailed.store(true, std::memory_order_release);
          return;
        }
        mine.push_back(*popped);
        consumedCount.fetch_add(1, std::memory_order_acq_rel);
      }
    });
  }

  std::vector<std::thread> producers;
  producers.reserve(producerCount);
  for (std::size_t p = 0; p < producerCount; ++p) {
    producers.emplace_back([&, p] {
      while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }

      for (std::size_t i = 0; i < perProducer; ++i) {
        if (!queue.wait_push(static_cast<std::uint32_t>(p * perProducer + i))) {
          pushFailed.store(true, std::memory_order_release);
          return;
        }
      }
    });
  }

  start.store(true, std::memory_order_release);
  for (std::thread& producer : producers) {
    producer.join();
  }
  for (std::thread& consumer : consumers) {
    consumer.join();
  }
  REQUIRE_FALSE(pushFailed.load(std::memory_order_acquire));
  REQUIRE_FALSE(popFailed.load(std::memory_order_acquire));
  REQUIRE(consumedCount.load(std::memory_order_acquire) == total);

  std::vector<std::uint32_t> all;
  all.reserve(total);
  for (const std::vector<std::uint32_t>& mine : consumed) {
    all.insert(all.end(), mine.begin(), mine.end());
  }

  REQUIRE(all.size() == total);
  std::sort(all.begin(), all.end());
  for (std::size_t i = 0; i < total; ++i) {
    REQUIRE(all[i] == i);
  }
  REQUIRE(queue.empty());
}

} // namespace pf::adapters
