#include <cstdint>
#include <optional>
#include <iostream>

#include <catch2/catch_test_macros.hpp>

import PixelForge.adapters.stack;

import PixelForge.validation_helpers;

#define BUF_SIZE 128

namespace pf::adapters {

namespace {

//pf_vh::LifeTimeTrackerStorage M_buf[BUF_SIZE];
//pf_vh::LifeTimeTracker* M_p_buf = reinterpret_cast<pf_vh::LifeTimeTracker*>(M_buf);

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

}
