#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

import PixelForge.views.ringQueue;

#define BUF_SIZE 128

namespace pf::views {

  TEST_CASE( "RingQueue basic" , "[views][RingQueue]" ) {
      
    std::uint32_t buf[BUF_SIZE]{};  
    RingQueue<std::uint32_t> queue(buf, BUF_SIZE);

    SECTION( "buffer untouched" ) {
      for (std::size_t i = 0; i < BUF_SIZE; i++) {
        REQUIRE(buf[i] == std::uint32_t{});
      }
    }

    SECTION( "basic properties" ) {
      REQUIRE(queue.capacity() == BUF_SIZE);
      REQUIRE(queue.size() == 0);
      REQUIRE(queue.empty());
      REQUIRE(!queue.full());
      REQUIRE(queue.data() == buf);
      REQUIRE(queue.remaining() == BUF_SIZE);
    }

    SECTION( "push/pop full" ) { // Extra verbose here will not be checking all of this later on
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
      REQUIRE(queue.size() == 0);
      REQUIRE(queue.remaining() == BUF_SIZE);
      REQUIRE(!queue.try_pop());
    }

  }
}
