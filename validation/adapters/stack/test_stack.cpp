#include <cstdint>
#include <optional>
#include <iostream>

#include <catch2/catch_test_macros.hpp>

import PixelForge.adapters.stack;

import PixelForge.validation_helpers;

#define BUF_SIZE 128

namespace pf::adapters {

namespace {

pf_vh::LifeTimeTrackerStorage M_buf[BUF_SIZE];
pf_vh::LifeTimeTracker* M_p_buf = reinterpret_cast<pf_vh::LifeTimeTracker*>(M_buf);

}

TEST_CASE( "Stack basic" , "[adapters][Stack]" ) {
    
  std::uint32_t buf[BUF_SIZE]{};  
  Stack<std::uint32_t> stack(buf, BUF_SIZE);

  SECTION( "buffer untouched" ) {
    for (std::size_t i = 0; i < BUF_SIZE; i++) {
      REQUIRE(buf[i] == std::uint32_t{});
    }
  }

  SECTION( "basic properties" ) {
    REQUIRE(stack.capacity() == BUF_SIZE);
    REQUIRE(stack.size() == 0);
    REQUIRE(stack.empty());
    REQUIRE(!stack.full());
    REQUIRE(stack.data() == buf);
    REQUIRE(stack.end() == buf + BUF_SIZE);
    REQUIRE(stack.remaining() == BUF_SIZE);
  }

  SECTION( "push/pop full" ) {
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
    
    std::cout << 3 << 'n';
    REQUIRE(stack.full());
    REQUIRE(stack.size() == BUF_SIZE);
    std::cout << 5 << 'n';
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
    REQUIRE(stack.size() == 0);
    REQUIRE(stack.remaining() == BUF_SIZE);
    REQUIRE(!stack.try_pop());
  }
}

TEST_CASE( "RingQueue lifetimes", "[adapters][RingQueue]" ) {
  
  SECTION( "single push, single pop" ) {

    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      Stack<pf_vh::LifeTimeTracker> stack(M_p_buf, BUF_SIZE);
      
      REQUIRE(stack.try_emplace().has_value());

      {
        pf_vh::LifeTimeTracker::OpInfo opInfo = {.id = 0, .type = pf_vh::LifeTimeTracker::OpType::DEFAULT_CONSTRUCT};   
        REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(M_p_buf)[0] == opInfo);
      }
      
      REQUIRE(stack.try_pop().has_value());
      {
        pf_vh::LifeTimeTracker::OpInfo opInfo = {.id = 0, .type = pf_vh::LifeTimeTracker::OpType::DESTRUCT};   
        REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(M_p_buf)[1] == opInfo);
      }
    }
    
    // check no extra frees here
    for (std::size_t i = 0; i < BUF_SIZE; i++) {
      REQUIRE(!pf_vh::LifeTimeTracker::opLogs().contains(&M_p_buf[i]));
    }

  }

  SECTION( "push to full, pop to empty" ) {

    {
      pf_vh::LifeTimeTracker::DeferClear clearer{};
      Stack<pf_vh::LifeTimeTracker> stack(M_p_buf, BUF_SIZE);

      for (std::size_t j = 0; j < 5; j++) {
      
        for (std::size_t i = 0; i < BUF_SIZE; i++) {
          REQUIRE(stack.try_emplace().has_value());
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).size() == 2 * j + 1);
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[i]).back().type == pf_vh::LifeTimeTracker::OpType::DEFAULT_CONSTRUCT);
          REQUIRE(stack.size() == i + 1);
        }
      
        REQUIRE(stack.full());

        for (std::size_t i = 0; i < BUF_SIZE; i++) {
          REQUIRE(stack.size() == BUF_SIZE - i);
          REQUIRE(stack.try_pop().has_value());
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[BUF_SIZE - i - 1]).size() == 2 * j + 2);
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(&M_p_buf[BUF_SIZE - i - 1]).back().type == pf_vh::LifeTimeTracker::OpType::DESTRUCT);
        }

        REQUIRE(stack.empty());
      }
    }

  }
}

}
