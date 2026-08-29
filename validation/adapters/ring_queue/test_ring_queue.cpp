#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <vector>

#include <catch2/catch_test_macros.hpp>

import PixelForge.adapters;

import PixelForge.validation_helpers;

constexpr auto BUF_SIZE = 128;

namespace pf::adapters {

namespace {

pf_vh::LifeTimeTrackerStorage M_buf[BUF_SIZE];
pf_vh::LifeTimeTracker* M_p_buf = reinterpret_cast<pf_vh::LifeTimeTracker*>(M_buf);

pf_vh::LifeTimeTrackerStorage M_buf2[BUF_SIZE];
pf_vh::LifeTimeTracker* M_p_buf2 = reinterpret_cast<pf_vh::LifeTimeTracker*>(M_buf2);

struct M_UnsizedInputRange {
  struct iterator {
    using iterator_category = std::input_iterator_tag;
    using value_type = std::uint32_t;
    using difference_type = std::ptrdiff_t;
    std::uint32_t v = 0;
    iterator&
    operator++() {
      return *this;
    }
    iterator
    operator++(int) {
      return *this;
    }
    const std::uint32_t&
    operator*() const {
      return v;
    }
    bool
    operator==(const iterator&) const = default;
  };
  iterator
  begin() const {
    return {};
  }
  iterator
  end() const {
    return {};
  }
};

static_assert(std::ranges::input_range<M_UnsizedInputRange>);
static_assert(!std::ranges::sized_range<M_UnsizedInputRange>);
static_assert(!CompatibleInputRange_c<RingQueue<std::uint32_t>, M_UnsizedInputRange>);
static_assert(
    CompatibleInputRange_c<RingQueue<std::uint32_t>, std::vector<std::uint32_t>>);

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
        pf_vh::LifeTimeTracker::OpInfo opInfo = {
            .id = 0, .type = pf_vh::LifeTimeTracker::OpType::DESTRUCT};
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
  alignas(
      std::unique_ptr<int>) unsigned char buf[BUF_SIZE * sizeof(std::unique_ptr<int>)]{};
  RingQueue<std::unique_ptr<int>> queue(reinterpret_cast<std::unique_ptr<int>*>(buf),
                                        BUF_SIZE);

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
    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[0])[1].type ==
            pf_vh::LifeTimeTracker::OpType::DESTRUCT);
    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[0]).back().type ==
            pf_vh::LifeTimeTracker::OpType::CONSTRUCT);

    pf_vh::LifeTimeTracker val{1};
    queue.force_push(std::move(val));
    REQUIRE(queue.size() == BUF_SIZE);
    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[1]).size() == 3);
    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[1])[1].type ==
            pf_vh::LifeTimeTracker::OpType::DESTRUCT);
    REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[1]).back().type ==
            pf_vh::LifeTimeTracker::OpType::MOVE_CONSTRUCT);

    for (std::size_t i = 2; i < BUF_SIZE; i++) {
      REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).size() == 1);
    }
  }
}

TEST_CASE("RingQueue push_range basic", "[adapters][RingQueue]") {
  std::uint32_t buf[BUF_SIZE]{};
  RingQueue<std::uint32_t> queue(buf, BUF_SIZE);

  std::vector<std::uint32_t> input{1, 2, 3, 4, 5};
  queue.push_range(input);

  REQUIRE(queue.size() == 5);
  for (std::size_t i = 0; i < 5; i++) {
    REQUIRE(queue.front() == static_cast<std::uint32_t>(i + 1));
    queue.pop_unchecked();
  }
  REQUIRE(queue.empty());

  SECTION("push_range full fails") {
    std::vector<std::uint32_t> big(BUF_SIZE + 1, 42);
    REQUIRE(!queue.try_push_range(big));
    REQUIRE(queue.empty());
  }

  SECTION("push_range_unchecked") {
    std::vector<std::uint32_t> input2{10, 20, 30};
    queue.push_range_unchecked(input2);
    REQUIRE(queue.size() == 3);
    REQUIRE(queue.front() == 10);
    REQUIRE(queue.back() == 30);
  }
}

TEST_CASE("RingQueue push_range move only", "[adapters][RingQueue]") {
  alignas(
      std::unique_ptr<int>) unsigned char buf[BUF_SIZE * sizeof(std::unique_ptr<int>)]{};
  RingQueue<std::unique_ptr<int>> queue(reinterpret_cast<std::unique_ptr<int>*>(buf),
                                        BUF_SIZE);

  std::vector<std::unique_ptr<int>> input;
  input.push_back(std::make_unique<int>(42));
  input.push_back(std::make_unique<int>(43));
  input.push_back(std::make_unique<int>(44));

  queue.push_range(std::move(input));

  REQUIRE(queue.size() == 3);
  auto v1 = queue.try_pop();
  REQUIRE(v1.has_value());
  REQUIRE(*v1.value() == 42);
  auto v2 = queue.try_pop();
  REQUIRE(v2.has_value());
  REQUIRE(*v2.value() == 43);
  auto v3 = queue.try_pop();
  REQUIRE(v3.has_value());
  REQUIRE(*v3.value() == 44);
  REQUIRE(queue.empty());

  SECTION("try_push_range") {
    std::vector<std::unique_ptr<int>> input2;
    input2.push_back(std::make_unique<int>(100));
    input2.push_back(std::make_unique<int>(200));
    REQUIRE(queue.try_push_range(std::move(input2)));
    REQUIRE(queue.size() == 2);
  }

  SECTION("push_range_unchecked") {
    std::vector<std::unique_ptr<int>> input3;
    input3.push_back(std::make_unique<int>(300));
    input3.push_back(std::make_unique<int>(400));
    queue.push_range_unchecked(std::move(input3));
    REQUIRE(queue.size() == 2);
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

TEST_CASE("RingQueue pow2 capacity validation", "[adapters][RingQueue]") {
  std::uint32_t buf[BUF_SIZE]{};
  REQUIRE_NOTHROW((RingQueue<std::uint32_t, true>(buf, BUF_SIZE)));
  REQUIRE_THROWS((RingQueue<std::uint32_t, true>(buf, BUF_SIZE - 1)));
}

}
