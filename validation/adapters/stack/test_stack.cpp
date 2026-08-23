#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

import PixelForge.adapters;

import PixelForge.validation_helpers;

constexpr auto BUF_SIZE = 128;

namespace pf::adapters {

namespace {

pf_vh::LifeTimeTrackerStorage M_buf[BUF_SIZE];
pf_vh::LifeTimeTracker* M_p_buf = reinterpret_cast<pf_vh::LifeTimeTracker*>(M_buf);

}

TEST_CASE("Stack basic", "[adapters][Stack]") {

  std::uint32_t buf[BUF_SIZE]{};
  Stack<std::uint32_t> stack(buf, BUF_SIZE);

  SECTION("buffer untouched") {
    for (unsigned int i : buf) {
      REQUIRE(i == std::uint32_t{});
    }
  }

  SECTION("basic properties") {
    REQUIRE(stack.capacity() == BUF_SIZE);
    REQUIRE(stack.empty());
    REQUIRE(stack.empty());
    REQUIRE(!stack.full());
    REQUIRE(stack.data() == buf);
    REQUIRE(stack.end() == buf + BUF_SIZE);
    REQUIRE(stack.remaining() == BUF_SIZE);
  }

  SECTION("push/pop full") {
    REQUIRE(stack.empty());
    REQUIRE(!stack.try_pop());
    std::uint32_t pushVal;

    for (std::size_t i = 0; i < BUF_SIZE; i++) {
      REQUIRE(stack.capacity() == BUF_SIZE);
      REQUIRE(stack.size() == i);
      REQUIRE(!stack.full());
      REQUIRE(stack.data() == buf);
      REQUIRE(stack.remaining() == BUF_SIZE - i);

      pushVal = static_cast<std::uint32_t>(BUF_SIZE + i);
      REQUIRE(stack.try_push(pushVal));
      REQUIRE(buf[i] == pushVal);
      REQUIRE(!stack.empty());
    }

    REQUIRE(stack.full());
    REQUIRE(stack.size() == BUF_SIZE);
    REQUIRE(stack.remaining() == 0);
    REQUIRE(!stack.try_push(0));

    for (std::size_t i = 0; i < BUF_SIZE; i++) {
      REQUIRE(stack.capacity() == BUF_SIZE);
      REQUIRE(stack.size() == BUF_SIZE - i);
      REQUIRE(stack.data() == buf);
      REQUIRE(stack.remaining() == i);

      std::optional<std::uint32_t> popVal = stack.try_pop();
      REQUIRE(popVal.has_value());
      REQUIRE(popVal.value() == BUF_SIZE * 2 - i - 1);
    }

    REQUIRE(stack.empty());
    REQUIRE(stack.empty());
    REQUIRE(stack.remaining() == BUF_SIZE);
    REQUIRE(!stack.try_pop());
  }
}

TEST_CASE("Stack lifetimes", "[adapters][Stack]") {

  SECTION("single push, single pop") {

    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      Stack<pf_vh::LifeTimeTracker> stack(M_p_buf, BUF_SIZE);

      REQUIRE(stack.try_emplace().has_value());

      {
        pf_vh::LifeTimeTracker::OpInfo opInfo = {
            .id = 0, .type = pf_vh::LifeTimeTracker::OpType::DEFAULT_CONSTRUCT};
        REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(M_p_buf)[0] == opInfo);
      }

      REQUIRE(stack.try_pop().has_value());
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

  SECTION("push to full, pop to empty") {

    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      Stack<pf_vh::LifeTimeTracker> stack(M_p_buf, BUF_SIZE);

      for (std::size_t j = 0; j < 5; j++) {

        for (std::size_t i = 0; i < BUF_SIZE; i++) {
          REQUIRE(stack.try_emplace().has_value());
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).size() == 2 * j + 1);
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).back().type ==
                  pf_vh::LifeTimeTracker::OpType::DEFAULT_CONSTRUCT);
          REQUIRE(stack.size() == i + 1);
        }

        REQUIRE(stack.full());

        for (std::size_t i = 0; i < BUF_SIZE; i++) {
          REQUIRE(stack.size() == BUF_SIZE - i);
          REQUIRE(stack.try_pop().has_value());
          REQUIRE(
              pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[BUF_SIZE - i - 1]).size() ==
              2 * j + 2);
          REQUIRE(pf_vh::LifeTimeTracker::opLogs()
                      .at(&M_p_buf[BUF_SIZE - i - 1])
                      .back()
                      .type == pf_vh::LifeTimeTracker::OpType::DESTRUCT);
        }

        REQUIRE(stack.empty());
      }
    }
  }
}

TEST_CASE("Stack top", "[adapters][Stack]") {
  std::uint32_t buf[BUF_SIZE]{};
  Stack<std::uint32_t> stack(buf, BUF_SIZE);

  for (std::size_t i = 0; i < 10; i++) {
    REQUIRE(stack.try_push(static_cast<std::uint32_t>(i + 1)));
    REQUIRE(stack.top() == i + 1);
    REQUIRE(stack.size() == i + 1);
  }

  const Stack<std::uint32_t>& constStack = stack;
  REQUIRE(constStack.top() == 10);

  REQUIRE(!stack.empty());
}

TEST_CASE("Stack clear and destructor destroy elements", "[adapters][Stack]") {

  SECTION("clear destroys all elements and stack stays reusable") {
    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      Stack<pf_vh::LifeTimeTracker> stack(M_p_buf, BUF_SIZE);

      for (std::size_t i = 0; i < 4; i++) {
        REQUIRE(stack.try_emplace(static_cast<int>(i)).has_value());
      }
      REQUIRE(stack.size() == 4);

      stack.clear();
      REQUIRE(stack.empty());
      REQUIRE(stack.size() == 0);

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

      // LIFO reuse after clear: slot order continues from the bottom
      REQUIRE(stack.try_emplace().has_value());
      REQUIRE(stack.size() == 1);
      REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(M_p_buf).size() == 3);
      REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(M_p_buf).back().type ==
              pf_vh::LifeTimeTracker::OpType::DEFAULT_CONSTRUCT);
    }
  }

  SECTION("destructor destroys remaining elements") {
    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      {
        Stack<pf_vh::LifeTimeTracker> stack(M_p_buf, BUF_SIZE);
        for (std::size_t i = 0; i < 4; i++) {
          REQUIRE(stack.try_emplace().has_value());
        }
        REQUIRE(stack.size() == 4);
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

TEST_CASE("Stack push_range basic", "[adapters][Stack]") {
  std::uint32_t buf[BUF_SIZE]{};
  Stack<std::uint32_t> stack(buf, BUF_SIZE);

  std::vector<std::uint32_t> input{1, 2, 3, 4, 5};
  stack.push_range(input);

  REQUIRE(stack.size() == 5);
  for (int expected = 5; expected > 0; expected--) {
    REQUIRE(stack.top() == static_cast<std::uint32_t>(expected));
    REQUIRE(stack.pop_unchecked() == static_cast<std::uint32_t>(expected));
  }
  REQUIRE(stack.empty());

  SECTION("push_range full fails") {
    std::vector<std::uint32_t> big(BUF_SIZE + 1, 42);
    REQUIRE_THROWS_AS(stack.push_range(big), Stack<std::uint32_t>::FullError);
    REQUIRE(stack.empty());
  }

  SECTION("try_push_range fails without side effects when too big") {
    std::vector<std::uint32_t> big(BUF_SIZE + 1, 42);
    REQUIRE(!stack.try_push_range(big));
    REQUIRE(stack.empty());

    std::vector<std::uint32_t> small{7, 8, 9};
    REQUIRE(stack.try_push_range(small));
    REQUIRE(stack.size() == 3);
    REQUIRE(stack.top() == 9);
  }

  SECTION("push_range_unchecked") {
    std::vector<std::uint32_t> input2{10, 20, 30};
    stack.push_range_unchecked(input2);
    REQUIRE(stack.size() == 3);
    REQUIRE(stack.top() == 30);
  }

  SECTION("push_range rvalue range moves elements") {
    std::vector<std::unique_ptr<int>> ptrs;
    ptrs.push_back(std::make_unique<int>(42));
    ptrs.push_back(std::make_unique<int>(43));

    std::unique_ptr<int> ptrBuf[BUF_SIZE]{};
    Stack<std::unique_ptr<int>> ptrStack(ptrBuf, BUF_SIZE);
    ptrStack.push_range(std::move(ptrs));

    REQUIRE(ptrs[0] == nullptr);
    REQUIRE(ptrs[1] == nullptr);
    REQUIRE(ptrStack.size() == 2);
    REQUIRE(*ptrStack.pop_unchecked() == 43);
    REQUIRE(*ptrStack.pop_unchecked() == 42);
  }
}

}
