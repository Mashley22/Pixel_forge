#include <cstdint>
#include <memory>
#include <optional>

#include <catch2/catch_test_macros.hpp>

import PixelForge.adapters.ringQueue;

import PixelForge.validation_helpers;

constexpr auto BUF_SIZE = 128;

namespace pf::adapters {

namespace {

pf_vh::LifeTimeTrackerStorage M_buf[BUF_SIZE];
pf_vh::LifeTimeTracker* M_p_buf = reinterpret_cast<pf_vh::LifeTimeTracker*>(M_buf);

pf_vh::LifeTimeTrackerStorage M_buf2[BUF_SIZE];
pf_vh::LifeTimeTracker* M_p_buf2 = reinterpret_cast<pf_vh::LifeTimeTracker*>(M_buf2);

}

TEST_CASE("RingQueue basic", "[adapters][RingQueue]") {

  std::uint32_t buf[BUF_SIZE]{};
  RingQueue<std::uint32_t> queue(buf, BUF_SIZE);

  SECTION("buffer untouched") {
    for (unsigned int i : buf) {
      REQUIRE(i == std::uint32_t{});
    }
  }

  SECTION("basic properties") {
    REQUIRE(queue.capacity() == BUF_SIZE);
    REQUIRE(queue.empty());
    REQUIRE(queue.empty());
    REQUIRE(!queue.full());
    REQUIRE(queue.data() == buf);
    REQUIRE(queue.remaining() == BUF_SIZE);
  }

  SECTION("push/pop full") {
    REQUIRE(queue.empty());
    REQUIRE(!queue.try_pop());
    std::uint32_t pushVal;

    for (std::size_t i = 0; i < BUF_SIZE; i++) {
      REQUIRE(queue.capacity() == BUF_SIZE);
      REQUIRE(queue.size() == i);
      REQUIRE(!queue.full());
      REQUIRE(queue.data() == buf);
      REQUIRE(queue.remaining() == BUF_SIZE - i);

      pushVal = static_cast<std::uint32_t>(BUF_SIZE + i);
      REQUIRE(queue.try_push(pushVal));
      REQUIRE(queue.back() == pushVal);
      REQUIRE(buf[i] == pushVal);
      REQUIRE(!queue.empty());
    }

    REQUIRE(queue.full());
    REQUIRE(queue.size() == BUF_SIZE);
    REQUIRE(queue.remaining() == 0);
    REQUIRE(!queue.try_push(0));

    for (std::size_t i = BUF_SIZE; i > 0; i--) {
      REQUIRE(queue.capacity() == BUF_SIZE);
      REQUIRE(queue.size() == i);
      REQUIRE(queue.data() == buf);
      REQUIRE(queue.remaining() == BUF_SIZE - i);

      std::optional<std::uint32_t> popVal = queue.try_pop();
      REQUIRE(popVal.has_value());
      REQUIRE(popVal.value() == BUF_SIZE * 2 - i);
    }

    REQUIRE(queue.empty());
    REQUIRE(queue.empty());
    REQUIRE(queue.remaining() == BUF_SIZE);
    REQUIRE(!queue.try_pop());
  }
}

TEST_CASE("RingQueue lifetimes", "[adapters][RingQueue]") {

  SECTION("single push, single pop") {

    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      RingQueue<pf_vh::LifeTimeTracker> queue(M_p_buf, BUF_SIZE);

      REQUIRE(queue.try_emplace().has_value());

      {
        pf_vh::LifeTimeTracker::OpInfo opInfo = {
          .id = 0, .type = pf_vh::LifeTimeTracker::OpType::DEFAULT_CONSTRUCT};
        REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(M_p_buf)[0] == opInfo);
      }

      REQUIRE(queue.try_pop().has_value());
      {
        pf_vh::LifeTimeTracker::OpInfo opInfo = {.id = 0,
                                                 .type = pf_vh::LifeTimeTracker::OpType::DESTRUCT};
        REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(M_p_buf)[1] == opInfo);
      }
    }

    // check no extra frees here
    for (std::size_t i = 0; i < BUF_SIZE; i++) {
      REQUIRE(!pf_vh::LifeTimeTracker::opLogs().contains(&M_p_buf[i]));
    }
  }

  SECTION("to full and beyond! (push to full, pop to empty etc.)") {

    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      RingQueue<pf_vh::LifeTimeTracker> queue(M_p_buf, BUF_SIZE);

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
      RingQueue<pf_vh::LifeTimeTracker> queue(M_p_buf, BUF_SIZE);

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
}

TEST_CASE("RingQueue move only elements", "[adapters][RingQueue]") {
  std::unique_ptr<int> buf[BUF_SIZE]{};
  RingQueue<std::unique_ptr<int>> queue(buf, BUF_SIZE);

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

TEST_CASE("RingQueue force ops destroy replaced elements", "[adapters][RingQueue]") {
  {
    pf_vh::LifeTimeTracker::DeferClear clearer{};
    RingQueue<pf_vh::LifeTimeTracker> queue(M_p_buf, BUF_SIZE);

    for (std::size_t i = 0; i < BUF_SIZE; i++) {
      REQUIRE(queue.try_emplace(1).has_value());
    }
    REQUIRE(queue.full());

    queue.force_emplace(1);
    REQUIRE(queue.size() == BUF_SIZE);
    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[0]).size() == 3);
    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[0]).back().type ==
            pf_vh::LifeTimeTracker::OpType::CONSTRUCT);

    pf_vh::LifeTimeTracker val{1};
    queue.force_push(std::move(val));
    REQUIRE(queue.size() == BUF_SIZE);
    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[1]).size() == 3);
    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[1]).back().type ==
            pf_vh::LifeTimeTracker::OpType::MOVE_CONSTRUCT);

    for (std::size_t i = 2; i < BUF_SIZE; i++) {
      REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).size() == 1);
    }
  }
}

TEST_CASE("RingQueue move assignment", "[adapters][RingQueue]") {
  {
    pf_vh::LifeTimeTracker::DeferClear clearer{};
    RingQueue<pf_vh::LifeTimeTracker> queue(M_p_buf, BUF_SIZE);
    RingQueue<pf_vh::LifeTimeTracker> other(M_p_buf2, BUF_SIZE);

    REQUIRE(queue.try_emplace(1).has_value());
    REQUIRE(queue.try_emplace(1).has_value());
    REQUIRE(other.try_emplace(1).has_value());

    queue = std::move(other);

    REQUIRE(queue.size() == 1);
    REQUIRE(other.empty());
    REQUIRE(queue.data() == M_p_buf2);
    REQUIRE(other.data() == M_p_buf);

    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[0]).size() == 2);
    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[0]).back().type ==
            pf_vh::LifeTimeTracker::OpType::DESTRUCT);
    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[1]).size() == 2);
    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[1]).back().type ==
            pf_vh::LifeTimeTracker::OpType::DESTRUCT);
    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf2[0]).size() == 1);
  }
}

}
