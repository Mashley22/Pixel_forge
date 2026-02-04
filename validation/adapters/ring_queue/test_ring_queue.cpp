#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

import PixelForge.adapters.ringQueue;

import PixelForge.validation_helpers;

#define BUF_SIZE 128

namespace pf::adapters {

namespace {

pf_vh::LifeTimeTrackerStorage M_buf[BUF_SIZE];
pf_vh::LifeTimeTracker* M_p_buf = reinterpret_cast<pf_vh::LifeTimeTracker*>(M_buf);

}

  TEST_CASE( "RingQueue basic" , "[adapters][RingQueue]" ) {
      
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

  TEST_CASE( "RingQueue lifetimes", "[adapters][RingQueue]" ) {
    
    SECTION( "simple" ) {

      {
        RingQueue<pf_vh::LifeTimeTracker> queue(M_p_buf, BUF_SIZE);
        
        queue.emplace_unchecked();

        {
          pf_vh::LifeTimeTracker::OpInfo opInfo = {.id = 0, .type = pf_vh::LifeTimeTracker::OpType::DEFAULT_CONSTRUCT};   
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(M_p_buf)[0] == opInfo);
        }
        
        REQUIRE(queue.try_pop().has_value());
        {
          pf_vh::LifeTimeTracker::OpInfo opInfo = {.id = 0, .type = pf_vh::LifeTimeTracker::OpType::DESTRUCT};   
          REQUIRE(pf_vh::LifeTimeTracker::opLogs().at(M_p_buf)[1] == opInfo);
        }
        pf_vh::LifeTimeTracker::clearOpLogs();
      }
      
      // check no extra frees here
      for (std::size_t i = 0; i < BUF_SIZE; i++) {
        REQUIRE(!pf_vh::LifeTimeTracker::opLogs().contains(&M_p_buf[i]));
      }

    }
  }
}
