import Benchpp;
import PixelForge.adapters.ringQueue;

#include <array>
#include <iostream>

#include <catch2/catch_all.hpp>

#define BUF_SIZE 1000
#define RUN_NUM 10000
#define RUN_SIZE 10000
#define TOLERANCE 1.05

namespace pf::adapters {

namespace {

class NoOpDestructor {
public:
  void* x = nullptr;

  NoOpDestructor() = default;

  NoOpDestructor(NoOpDestructor&& other) : x(nullptr) {
    other.x = nullptr;
  }

  NoOpDestructor& operator=(NoOpDestructor&& other) {
    x = other.x;
    other.x = nullptr;
    return *this;
  }

  ~NoOpDestructor() {
    if (x != nullptr) {
     x = nullptr;
    }
  }
};

class NoOpAfterMoveDestructor {
public:
  void* x = nullptr;

  NoOpAfterMoveDestructor() = default;

  NoOpAfterMoveDestructor(NoOpAfterMoveDestructor&& other) : x(nullptr) {
    other.x = nullptr;
  }

  NoOpAfterMoveDestructor& operator=(NoOpAfterMoveDestructor&& other) {
    x = other.x;
    other.x = nullptr;
    return *this;
  }

  ~NoOpAfterMoveDestructor() {
    if (x != nullptr) {
      std::cout << 1;
      x = static_cast<void*>(new int[1000]);
    }
  }
};

static_assert(sizeof(NoOpDestructor) == sizeof(NoOpAfterMoveDestructor));
static_assert(sizeof(NoOpDestructor) == sizeof(void*));

std::array<void*, BUF_SIZE> noOpBuf;
std::array<void*, BUF_SIZE> otherBuf;

TEST_CASE( "validate destructor is not called if no op", "[adapters][RingQueue]" ) {

  RingQueue<NoOpDestructor> noOp(reinterpret_cast<NoOpDestructor*>(noOpBuf.data()), BUF_SIZE);
  RingQueue<NoOpAfterMoveDestructor> noOpAfterMove(reinterpret_cast<NoOpAfterMoveDestructor*>(otherBuf.data()), BUF_SIZE);

  benchpp::Timer noOpTimer;
  benchpp::Timer noOpAfterMoveTimer;

  for (std::size_t i = 0; i < RUN_NUM; i++) {
    noOpTimer.start();
    for (std::size_t j = 0; j < RUN_SIZE; j++) {
      noOp.emplace_unchecked();
      auto x = noOp.pop_unchecked();
      x.x = nullptr;
    }
    noOpTimer.stop();
    noOpTimer.recordAndReset();

    noOpAfterMoveTimer.start();
    for (std::size_t j = 0; j < RUN_SIZE; j++) {
      noOpAfterMove.emplace_unchecked();
      auto x = noOpAfterMove.pop_unchecked();
    }
    noOpAfterMoveTimer.stop();
    noOpAfterMoveTimer.recordAndReset();

  }
  
  auto noOpStats = benchpp::Stats<benchpp::TimeCount_t>::generate(noOpTimer.times());
  auto otherStats = benchpp::Stats<benchpp::TimeCount_t>::generate(noOpAfterMoveTimer.times());

  std::cout << otherStats.mean << '\n';
  std::cout << noOpStats.mean << '\n';
  REQUIRE_FALSE(true);
  REQUIRE(otherStats.mean < noOpStats.mean);
}

}

}
