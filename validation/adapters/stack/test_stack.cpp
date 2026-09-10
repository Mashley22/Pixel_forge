#include <cstdint>
#include <memory>
#include <optional>
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

alignas(pf_vh::LifeTimeTracker)
    std::array<std::byte, BUF_SIZE * sizeof(pf_vh::LifeTimeTracker)> M_buf2;
auto M_buf2_storage = Buffer::from(M_buf2).asObjects<pf_vh::LifeTimeTracker>(BUF_SIZE);

}

PF_TEST_CASE("basic", "[adapters][Stack]") {

  alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);
  Stack<std::uint32_t> stack(storage);

  SECTION("buffer untouched") {
    for (std::byte ele : buf) {
      REQUIRE(ele == std::byte{});
    }
  }

  SECTION("basic properties") {
    REQUIRE(stack.capacity() == BUF_SIZE);
    REQUIRE(stack.empty());
    REQUIRE(stack.empty());
    REQUIRE(!stack.full());
    REQUIRE(stack.data() == storage.data);
    REQUIRE(stack.end() == static_cast<std::uint32_t*>(storage.data) + BUF_SIZE);
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
      REQUIRE(stack.data() == storage.data);
      REQUIRE(stack.remaining() == BUF_SIZE - i);

      pushVal = static_cast<std::uint32_t>(BUF_SIZE + i);
      REQUIRE(stack.try_push(pushVal));
      REQUIRE(storage[i] == pushVal);
      REQUIRE(!stack.empty());
    }

    REQUIRE(stack.full());
    REQUIRE(stack.size() == BUF_SIZE);
    REQUIRE(stack.remaining() == 0);
    REQUIRE(!stack.try_push(0));

    for (std::size_t i = 0; i < BUF_SIZE; i++) {
      REQUIRE(stack.capacity() == BUF_SIZE);
      REQUIRE(stack.size() == BUF_SIZE - i);
      REQUIRE(stack.data() == storage.data);
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

PF_TEST_CASE("lifetimes", "[adapters][Stack]") {

  SECTION("single push, single pop") {

    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      Stack<pf_vh::LifeTimeTracker> stack(M_buf_storage);

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
      auto storage = Buffer::from(M_buf).asObjects<pf_vh::LifeTimeTracker>(BUF_SIZE);
      Stack<pf_vh::LifeTimeTracker> stack(storage);

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

PF_TEST_CASE("top", "[adapters][Stack]") {
  alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);
  Stack<std::uint32_t> stack(storage);

  for (std::size_t i = 0; i < 10; i++) {
    REQUIRE(stack.try_push(static_cast<std::uint32_t>(i + 1)));
    REQUIRE(stack.top() == i + 1);
    REQUIRE(stack.size() == i + 1);
  }

  const Stack<std::uint32_t>& constStack = stack;
  REQUIRE(constStack.top() == 10);

  REQUIRE(!stack.empty());
}

PF_TEST_CASE("clear and destructor destroy elements", "[adapters][Stack]") {

  SECTION("clear destroys all elements and stack stays reusable") {
    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      auto storage = Buffer::from(M_buf).asObjects<pf_vh::LifeTimeTracker>(BUF_SIZE);
      Stack<pf_vh::LifeTimeTracker> stack(storage);

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
        Stack<pf_vh::LifeTimeTracker> stack(M_buf_storage);
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

PF_TEST_CASE("push_range basic", "[adapters][Stack]") {
  alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);

  Stack<std::uint32_t> stack(storage);

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

    alignas(std::unique_ptr<int>) unsigned char
        ptrBuf[BUF_SIZE * sizeof(std::unique_ptr<int>)]{};
    Stack<std::unique_ptr<int>> ptrStack(
        ObjectStorage<std::unique_ptr<int>>{ptrBuf, BUF_SIZE});
    ptrStack.push_range(std::move(ptrs));

    REQUIRE(ptrs[0] == nullptr);
    REQUIRE(ptrs[1] == nullptr);
    REQUIRE(ptrStack.size() == 2);
    REQUIRE(*ptrStack.pop_unchecked() == 43);
    REQUIRE(*ptrStack.pop_unchecked() == 42);
  }
}

PF_TEST_CASE("move construction", "[adapters][Stack]") {
  alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);

  SECTION("moved stack owns elements, source is empty") {
    Stack<std::uint32_t> original(storage);
    REQUIRE(original.empty());

    for (std::size_t i = 0; i < 10; i++) {
      REQUIRE(original.try_push(static_cast<std::uint32_t>(i + 1)));
    }
    REQUIRE(original.size() == 10);

    Stack<std::uint32_t> moved(std::move(original));
    REQUIRE(moved.size() == 10);
    REQUIRE(original.empty());
    REQUIRE(original.data() == nullptr);

    for (std::size_t i = 0; i < 10; i++) {
      REQUIRE(moved.top() == static_cast<std::uint32_t>(10 - i));
      REQUIRE(moved.pop_unchecked() == static_cast<std::uint32_t>(10 - i));
    }
    REQUIRE(moved.empty());
  }

  SECTION("moved-from stack is safe to destroy") {
    [[maybe_unused]] Stack<std::uint32_t>* movedFrom = nullptr;
    {
      auto storage2 = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);
      Stack<std::uint32_t> original(storage2);
      original.try_push(42);
      movedFrom = &original;
      Stack<std::uint32_t> moved(std::move(original));
      REQUIRE(moved.size() == 1);
    }
  }

  SECTION("move construction preserves buffer integrity") {
    Stack<std::uint32_t> original(storage);
    original.try_push(1);
    original.try_push(2);
    original.try_push(3);

    Stack<std::uint32_t> moved(std::move(original));
    REQUIRE(moved.data() == storage.data);
    REQUIRE(moved.capacity() == BUF_SIZE);
    REQUIRE(moved.size() == 3);
    REQUIRE(original.data() == nullptr);
    REQUIRE(original.empty());
  }
}

PF_TEST_CASE("move assignment", "[adapters][Stack]") {
  alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
  alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf2{};
  auto storage1 = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);
  auto storage2 = Buffer::from(buf2).asObjects<std::uint32_t>(BUF_SIZE);

  SECTION("move assigned stack takes ownership, source is emptied") {
    Stack<std::uint32_t> stack1(storage1);
    Stack<std::uint32_t> stack2(storage2);

    for (std::size_t i = 0; i < 5; i++) {
      stack1.try_push(static_cast<std::uint32_t>(i + 1));
    }
    REQUIRE(stack1.size() == 5);
    REQUIRE(stack2.empty());

    stack2 = std::move(stack1);
    REQUIRE(stack2.size() == 5);
    REQUIRE(stack1.empty());
    REQUIRE(stack1.data() == nullptr);

    for (std::size_t i = 0; i < 5; i++) {
      REQUIRE(stack2.top() == static_cast<std::uint32_t>(5 - i));
      REQUIRE(stack2.pop_unchecked() == static_cast<std::uint32_t>(5 - i));
    }
    REQUIRE(stack2.empty());
  }

  SECTION("move assignment destroys existing elements in destination") {
    Stack<std::uint32_t> stack1(storage1);
    Stack<std::uint32_t> stack2(storage2);

    stack1.try_push(100);
    stack1.try_push(200);

    stack2.try_push(1);
    stack2.try_push(2);
    stack2.try_push(3);

    stack2 = std::move(stack1);
    REQUIRE(stack2.size() == 2);
    REQUIRE(stack2.top() == 200);
    REQUIRE(stack2.pop_unchecked() == 200);
    REQUIRE(stack2.top() == 100);
  }

  SECTION("move assignment is safe when source is empty") {
    Stack<std::uint32_t> stack1(storage1);
    Stack<std::uint32_t> stack2(storage2);

    stack2.try_push(1);
    stack2.try_push(2);

    stack2 = std::move(stack1);
    REQUIRE(stack2.empty());
    REQUIRE(stack2.data() == storage1.data);
    REQUIRE(stack2.capacity() == BUF_SIZE);
    REQUIRE(stack1.data() == nullptr);
    REQUIRE(stack1.empty());
  }
}

PF_TEST_CASE("copy construction is deleted", "[adapters][Stack]") {
  alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
  auto storage = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);
  Stack<std::uint32_t> stack(storage);
  stack.try_push(42);

  SECTION("copy constructor is deleted") {
    REQUIRE_NOTHROW((std::is_copy_constructible_v<Stack<std::uint32_t>> == false));
  }

  SECTION("move is not deleted") {
    REQUIRE(std::is_move_constructible_v<Stack<std::uint32_t>>);
    REQUIRE(std::is_move_assignable_v<Stack<std::uint32_t>>);
  }
}

PF_TEST_CASE("copy assignment", "[adapters][Stack]") {
  alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf{};
  auto storage1 = Buffer::from(buf).asObjects<std::uint32_t>(BUF_SIZE);

  SECTION("copies all elements from source") {
    Stack<std::uint32_t> source(storage1);
    for (std::size_t i = 0; i < 5; i++) {
      source.try_push(static_cast<std::uint32_t>(i + 1));
    }
    REQUIRE(source.size() == 5);

    alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf2{};
    auto storage2 = Buffer::from(buf2).asObjects<std::uint32_t>(BUF_SIZE);
    Stack<std::uint32_t> dest(storage2);

    dest = source;
    REQUIRE(dest.size() == 5);
    REQUIRE(source.size() == 5);

    for (std::size_t i = 0; i < 5; i++) {
      REQUIRE(dest.top() == static_cast<std::uint32_t>(5 - i));
      REQUIRE(dest.pop_unchecked() == static_cast<std::uint32_t>(5 - i));
    }
    REQUIRE(dest.empty());
  }

  SECTION("source and dest are independent") {
    Stack<std::uint32_t> source(storage1);
    source.try_push(100);
    source.try_push(200);

    alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf2{};
    auto storage2 = Buffer::from(buf2).asObjects<std::uint32_t>(BUF_SIZE);
    Stack<std::uint32_t> dest(storage2);
    dest.try_push(1);

    dest = source;
    REQUIRE(dest.size() == 2);
    REQUIRE(dest.top() == 200);
    REQUIRE(dest.pop_unchecked() == 200);
    REQUIRE(dest.pop_unchecked() == 100);
    REQUIRE(dest.empty());

    // source is unaffected
    REQUIRE(source.size() == 2);
    REQUIRE(source.top() == 200);
  }

  SECTION("destroys existing elements in destination") {
    Stack<std::uint32_t> source(storage1);
    source.try_push(1);
    source.try_push(2);

    alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf2{};
    auto storage2 = Buffer::from(buf2).asObjects<std::uint32_t>(BUF_SIZE);
    Stack<std::uint32_t> dest(storage2);
    dest.try_push(99);
    REQUIRE(dest.size() == 1);

    dest = source;
    REQUIRE(dest.size() == 2);
    REQUIRE(dest.top() == 2);
  }

  SECTION("copy assignment of empty source empties destination") {
    Stack<std::uint32_t> source(storage1);

    alignas(std::uint32_t) std::array<std::byte, BUF_SIZE * sizeof(std::uint32_t)> buf2{};
    auto storage2 = Buffer::from(buf2).asObjects<std::uint32_t>(BUF_SIZE);
    Stack<std::uint32_t> dest(storage2);
    dest.try_push(1);
    dest.try_push(2);

    dest = source;
    REQUIRE(dest.empty());
  }
}

}
