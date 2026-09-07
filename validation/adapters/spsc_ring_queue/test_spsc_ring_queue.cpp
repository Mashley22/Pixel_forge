#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#include <PixelForgeValidationHelpers/helpers.hpp>
#include <catch2/catch_test_macros.hpp>

import PixelForge.adapters;
import PixelForge.core;

import PixelForge.validation_helpers;

constexpr auto BUF_SIZE = 128;

namespace pf::adapters {

namespace {

alignas(pf_vh::LifeTimeTracker)
    std::array<std::byte, BUF_SIZE * sizeof(pf_vh::LifeTimeTracker)> M_buf;
pf_vh::LifeTimeTracker* M_p_buf = reinterpret_cast<pf_vh::LifeTimeTracker*>(M_buf.data());
auto M_buf_storage = Buffer::from(M_buf).asObjects<pf_vh::LifeTimeTracker>(BUF_SIZE);

}

PF_TEST_CASE("basic", "[adapters][SPSCQueue]") {

  alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);
  SPSCRingQueue<std::uint32_t> queue(storage);

  SECTION("buffer untouched") {
    for (std::byte ele : buf) {
      REQUIRE(ele == std::byte{});
    }
  }

  SECTION("basic properties") {
    REQUIRE(queue.capacity() == BUF_SIZE);
    REQUIRE(queue.empty());
    REQUIRE(!queue.full());
    REQUIRE(queue.data() == storage.data);
    REQUIRE(queue.size() == 0);
    REQUIRE(queue.remaining() == BUF_SIZE);
  }

  SECTION("span constructor") {
    std::span<std::byte> spanBuf(buf);
    SPSCRingQueue<std::uint32_t> spanQueue(
        Buffer::from(spanBuf).asObjects<std::uint32_t>(BUF_SIZE));
    REQUIRE(spanQueue.capacity() == BUF_SIZE);
    REQUIRE(spanQueue.data() == storage.data);
    REQUIRE(spanQueue.empty());
  }

  SECTION("size and remaining tracking") {
    for (std::size_t i = 0; i < BUF_SIZE; i++) {
      REQUIRE(queue.size() == i);
      REQUIRE(queue.remaining() == BUF_SIZE - i);
      REQUIRE(!queue.full());
      REQUIRE(queue.try_push(static_cast<std::uint32_t>(i)));
    }

    REQUIRE(queue.full());
    REQUIRE(queue.size() == BUF_SIZE);
    REQUIRE(queue.remaining() == 0);

    for (std::size_t i = 0; i < BUF_SIZE; i++) {
      REQUIRE(queue.size() == BUF_SIZE - i);
      REQUIRE(queue.remaining() == i);
      std::optional<std::uint32_t> val = queue.try_pop();
      REQUIRE(val.has_value());
    }

    REQUIRE(queue.empty());
    REQUIRE(queue.size() == 0);
    REQUIRE(queue.remaining() == BUF_SIZE);
  }

  SECTION("front and back") {
    for (std::size_t i = 0; i < 10; i++) {
      REQUIRE(queue.try_push(static_cast<std::uint32_t>(i + 1)));
      REQUIRE(!queue.empty());
      REQUIRE(queue.front() == 1);
      // NOTE: only safe while head has not wrapped past capacity
      REQUIRE(queue.back() == i + 1);
    }
    REQUIRE(queue.size() == 10);
  }
}

PF_TEST_CASE("fifo order across wraps", "[adapters][SPSCQueue]") {

  constexpr auto ROUNDS = 5;

  alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);
  SPSCRingQueue<std::uint32_t> queue(storage);

  SECTION("full rounds") {
    std::uint32_t nextValue = 0;

    for (int round = 0; round < ROUNDS; round++) {
      for (std::size_t i = 0; i < BUF_SIZE; i++) {
        REQUIRE(queue.try_push(nextValue));
        nextValue++;
      }

      REQUIRE(queue.full());
      REQUIRE(!queue.try_push(0));

      for (std::size_t i = 0; i < BUF_SIZE; i++) {
        const auto expected =
            static_cast<std::uint32_t>(round * BUF_SIZE + static_cast<int>(i));
        std::optional<std::uint32_t> val = queue.try_pop();
        REQUIRE(val.has_value());
        REQUIRE(val.value() == expected);
      }

      REQUIRE(queue.empty());
      REQUIRE(!queue.try_pop().has_value());
    }
  }

  SECTION("interleaved push pop across wraps") {
    std::uint32_t pushed = 0;
    std::uint32_t popped = 0;

    for (int round = 0; round < 100; round++) {
      for (int j = 0; j < 3; j++) {
        REQUIRE(queue.try_push(pushed));
        pushed++;
      }

      for (int j = 0; j < 2; j++) {
        std::optional<std::uint32_t> val = queue.try_pop();
        REQUIRE(val.has_value());
        REQUIRE(val.value() == popped);
        popped++;
      }
    }

    while (!queue.empty()) {
      std::optional<std::uint32_t> val = queue.try_pop();
      REQUIRE(val.has_value());
      REQUIRE(val.value() == popped);
      popped++;
    }

    REQUIRE(pushed == popped);
    REQUIRE(queue.empty());
  }
}

PF_TEST_CASE("error policies", "[adapters][SPSCQueue]") {

  using Queue = SPSCRingQueue<std::uint32_t>;

  alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);
  Queue queue(storage);

  SECTION("pop on empty") {
    REQUIRE(!queue.try_pop().has_value());
    REQUIRE_THROWS_AS(queue.pop(), Queue::EmptyError);
    REQUIRE(queue.empty());
  }

  SECTION("push on full") {
    for (std::size_t i = 0; i < BUF_SIZE; i++) {
      REQUIRE(queue.try_push(static_cast<std::uint32_t>(i)));
    }
    REQUIRE(queue.full());

    REQUIRE(!queue.try_push(0));
    REQUIRE_THROWS_AS(queue.push(std::uint32_t{9}), Queue::FullError);
    REQUIRE_THROWS_AS(queue.emplace(std::uint32_t{9}), Queue::FullError);

    REQUIRE(queue.size() == BUF_SIZE);
    REQUIRE(queue.front() == 0);
  }

  SECTION("unchecked ops") {
    queue.push_unchecked(std::uint32_t{1});
    const std::uint32_t* ptr = queue.emplace_unchecked(std::uint32_t{2});

    REQUIRE(ptr != nullptr);
    REQUIRE(*ptr == 2);
    REQUIRE(queue.size() == 2);

    REQUIRE(queue.pop_unchecked() == 1);
    REQUIRE(queue.pop_unchecked() == 2);
    REQUIRE(queue.empty());
  }

  SECTION("emplace returns element pointer") {
    std::optional<std::uint32_t*> ptr = queue.try_emplace(std::uint32_t{7});

    REQUIRE(ptr.has_value());
    REQUIRE(ptr.value() >= reinterpret_cast<std::uint32_t*>(buf.data()));
    REQUIRE(ptr.value() < reinterpret_cast<std::uint32_t*>(buf.data() + BUF_SIZE));
    REQUIRE(*ptr.value() == 7);
    REQUIRE(ptr.value() == &queue.front());

    REQUIRE(queue.pop_unchecked() == 7);
    REQUIRE(queue.empty());
  }
}

PF_TEST_CASE("lifetimes", "[adapters][SPSCQueue]") {

  SECTION("single push, single pop") {

    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      SPSCRingQueue<pf_vh::LifeTimeTracker> queue(M_buf_storage);

      REQUIRE(queue.try_emplace().has_value());

      {
        pf_vh::LifeTimeTracker::OpInfo opInfo = {
            .id = 0, .type = pf_vh::LifeTimeTracker::OpType::DEFAULT_CONSTRUCT};
        REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(M_p_buf)[0] == opInfo);
      }

      REQUIRE(queue.try_pop().has_value());
      {
        pf_vh::LifeTimeTracker::OpInfo opInfo = {
            .id = 0, .type = pf_vh::LifeTimeTracker::OpType::DESTRUCT};
        REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(M_p_buf)[1] == opInfo);
      }

      for (std::size_t i = 1; i < BUF_SIZE; i++) {
        REQUIRE(!pf_vh::LifeTimeTracker::opLogs().contains(&M_p_buf[i]));
      }
    }
  }

  SECTION("to full and beyond! (push to full, pop to empty etc.)") {

    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      SPSCRingQueue<pf_vh::LifeTimeTracker> queue(M_buf_storage);

      for (std::size_t j = 0; j < 5; j++) {

        for (std::size_t i = 0; i < BUF_SIZE; i++) {
          REQUIRE(queue.try_emplace().has_value());
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).size() == 2 * j + 1);
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).back().type ==
                  pf_vh::LifeTimeTracker::OpType::DEFAULT_CONSTRUCT);
          REQUIRE(queue.size() == i + 1);
        }

        REQUIRE(queue.full());

        for (std::size_t i = 0; i < BUF_SIZE; i++) {
          REQUIRE(queue.size() == BUF_SIZE - i);
          REQUIRE(queue.try_pop().has_value());
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).size() == 2 * j + 2);
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).back().type ==
                  pf_vh::LifeTimeTracker::OpType::DESTRUCT);
        }

        REQUIRE(queue.empty());
      }
    }
  }

  SECTION("to full and beyond! Alternating push pop") {

    {
      pf_vh::LifeTimeTracker::DeferClear clearer;
      SPSCRingQueue<pf_vh::LifeTimeTracker> queue(M_buf_storage);

      for (std::size_t j = 0; j < 5; j++) {

        for (std::size_t i = 0; i < BUF_SIZE; i++) {
          REQUIRE(queue.try_emplace().has_value());
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).size() == 2 * j + 1);
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).back().type ==
                  pf_vh::LifeTimeTracker::OpType::DEFAULT_CONSTRUCT);

          REQUIRE(queue.try_pop().has_value());
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).size() == 2 * j + 2);
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).back().type ==
                  pf_vh::LifeTimeTracker::OpType::DESTRUCT);
        }

        REQUIRE(queue.empty());
      }
    }
  }

  SECTION("clear destroys elements and queue stays reusable") {

    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      SPSCRingQueue<pf_vh::LifeTimeTracker> queue(M_buf_storage);

      for (std::size_t i = 0; i < 4; i++) {
        REQUIRE(queue.try_emplace(static_cast<int>(i)).has_value());
      }
      REQUIRE(queue.size() == 4);

      queue.clear();
      REQUIRE(queue.empty());
      REQUIRE(queue.size() == 0);

      for (std::size_t i = 0; i < BUF_SIZE; i++) {
        if (i < 4) {
          const auto& log = pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]);
          REQUIRE(log.size() == 2);
          REQUIRE(log[0].type == pf_vh::LifeTimeTracker::OpType::CONSTRUCT);
          REQUIRE(log.back().type == pf_vh::LifeTimeTracker::OpType::DESTRUCT);
        } else {
          REQUIRE(!pf_vh::LifeTimeTracker::opLogs().contains(&M_p_buf[i]));
        }
      }

      REQUIRE(queue.try_emplace().has_value());
      REQUIRE(queue.size() == 1);
      // head kept increasing past clear(), so the new element lands in slot 4
      const auto& reuseLog = pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[4]);
      REQUIRE(reuseLog.size() == 1);
      REQUIRE(reuseLog.back().type == pf_vh::LifeTimeTracker::OpType::DEFAULT_CONSTRUCT);
    }
  }

  SECTION("destructor destroys remaining elements") {

    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      {
        SPSCRingQueue<pf_vh::LifeTimeTracker> queue(M_buf_storage);
        for (std::size_t i = 0; i < 4; i++) {
          REQUIRE(queue.try_emplace().has_value());
        }
        REQUIRE(queue.size() == 4);
      }

      for (std::size_t i = 0; i < BUF_SIZE; i++) {
        if (i < 4) {
          const auto& log = pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]);
          REQUIRE(log.size() == 2);
          REQUIRE(log[0].type == pf_vh::LifeTimeTracker::OpType::DEFAULT_CONSTRUCT);
          REQUIRE(log.back().type == pf_vh::LifeTimeTracker::OpType::DESTRUCT);
        } else {
          REQUIRE(!pf_vh::LifeTimeTracker::opLogs().contains(&M_p_buf[i]));
        }
      }
    }
  }
}

PF_TEST_CASE("move only elements", "[adapters][SPSCQueue]") {
  alignas(std::unique_ptr<int>)
      std::array<std::byte, BUF_SIZE * sizeof(std::unique_ptr<int>)>
          buf;
  auto storage = Buffer::from(buf).asObjects<std::unique_ptr<int>>(BUF_SIZE);
  SPSCRingQueue<std::unique_ptr<int>> queue(storage);

  REQUIRE(queue.try_push(std::make_unique<int>(42)));
  REQUIRE(queue.try_push(std::make_unique<int>(43)));
  REQUIRE(queue.size() == 2);

  auto val = queue.try_pop();
  REQUIRE(val.has_value());
  REQUIRE(*val.value() == 42);

  val = queue.try_pop();
  REQUIRE(val.has_value());
  REQUIRE(*val.value() == 43);

  REQUIRE(queue.empty());
}

PF_TEST_CASE("construction validation", "[adapters][SPSCQueue]") {

  SECTION("null buffer rejected") {
    ObjectStorage<std::uint32_t> storage{.data = nullptr, .size = BUF_SIZE};
    REQUIRE_THROWS((SPSCRingQueue<std::uint32_t>(storage)));
  }

  SECTION("zero capacity rejected") {
    alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
    ObjectStorage<std::uint32_t> storage{.data = buf.data(), .size = 0};
    REQUIRE_THROWS((SPSCRingQueue<std::uint32_t>(storage)));
  }

  SECTION("misaligned buffer rejected") {
    alignas(std::uint32_t) unsigned char raw[2 * sizeof(std::uint32_t)]{};
    ObjectStorage<std::uint32_t> storage{.data = static_cast<void*>(raw + 1), .size = 2};
    REQUIRE_THROWS((SPSCRingQueue<std::uint32_t>(storage)));
  }

  SECTION("power of two enforced only when requested") {
    alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
    auto storage = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);
    REQUIRE_NOTHROW((SPSCRingQueue<std::uint32_t, true>(storage)));
    REQUIRE_NOTHROW((SPSCRingQueue<std::uint32_t>(storage)));
    REQUIRE_THROWS((SPSCRingQueue<std::uint32_t, true>(
        Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE - 1))));
  }
}

PF_TEST_CASE("non power of two capacity", "[adapters][SPSCQueue]") {
  constexpr auto CAPACITY = 100;

  alignas(std::uint32_t) std::array<std::byte, CAPACITY * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(CAPACITY);
  SPSCRingQueue<std::uint32_t> queue(storage);
  REQUIRE(queue.capacity() == CAPACITY);

  std::uint32_t nextValue = 0;

  for (int round = 0; round < 3; round++) {
    for (int i = 0; i < CAPACITY; i++) {
      REQUIRE(queue.try_push(nextValue));
      nextValue++;
    }

    REQUIRE(queue.full());
    REQUIRE(!queue.try_push(0));

    for (int i = 0; i < CAPACITY; i++) {
      const auto expected = static_cast<std::uint32_t>(round * CAPACITY + i);
      std::optional<std::uint32_t> val = queue.try_pop();
      REQUIRE(val.has_value());
      REQUIRE(val.value() == expected);
    }

    REQUIRE(queue.empty());
  }
}

PF_TEST_CASE("concurrent producer consumer", "[adapters][SPSCQueue]") {

  constexpr auto COUNT = 20000;

  alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);
  SPSCRingQueue<std::uint32_t> queue(storage);

  std::vector<std::uint32_t> consumed;
  consumed.reserve(COUNT);

  std::thread producer([&queue] {
    for (std::uint32_t v = 0; v < COUNT; ++v) {
      while (!queue.try_push(v)) {
        std::this_thread::yield();
      }
    }
  });

  std::thread consumer([&queue, &consumed] {
    for (std::uint32_t expected = 0; expected < COUNT; ++expected) {
      std::optional<std::uint32_t> val = queue.try_pop();
      while (!val.has_value()) {
        std::this_thread::yield();
        val = queue.try_pop();
      }
      consumed.push_back(val.value());
    }
  });

  producer.join();
  consumer.join();

  REQUIRE(consumed.size() == COUNT);
  for (std::size_t i = 0; i < COUNT; i++) {
    REQUIRE(consumed[i] == static_cast<std::uint32_t>(i));
  }
  REQUIRE(queue.empty());
}

}
