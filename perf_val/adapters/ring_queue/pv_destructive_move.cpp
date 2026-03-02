import Benchpp;
import PixelForge.adapters.ringQueue;

#include <array>
#include <cmath>
#include <iostream>
#include <new>

#include <catch2/catch_all.hpp>

#include <PixelForge/core/macros.hpp>

#define BUF_SIZE 1000
#define RUN_NUM 10000
#define RUN_SIZE 10000
#define TOLERANCE 5

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

  ~NoOpDestructor() = default;
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

benchpp::Timer noOpTimer;
benchpp::Timer noOpAfterMoveTimer;

PF_ATTRIB_NOINLINE_CACHE_LINE_ALIGN
void 
noOpRun(RingQueue<NoOpDestructor>& rq) {
  noOpTimer.start();
  for (std::size_t j = 0; j < RUN_SIZE; j++) {
    rq.emplace_unchecked();
    auto x = rq.pop_unchecked();
    (void)x;
  }
  noOpTimer.stop();
  noOpTimer.recordAndReset();
}

PF_ATTRIB_NOINLINE_CACHE_LINE_ALIGN
void 
noOpAfterMoveRun(RingQueue<NoOpAfterMoveDestructor>& rq) {
  noOpAfterMoveTimer.start();
  for (std::size_t j = 0; j < RUN_SIZE; j++) {
    rq.emplace_unchecked();
    auto x = rq.pop_unchecked();
    (void)x;
  }
  noOpAfterMoveTimer.stop();
  noOpAfterMoveTimer.recordAndReset();
}

std::array<void*, BUF_SIZE> noOpBuf;
std::array<void*, BUF_SIZE> otherBuf;

TEST_CASE( "validate destructor is not called if no op", "[adapters][RingQueue]" ) {

  RingQueue<NoOpDestructor> noOp(reinterpret_cast<NoOpDestructor*>(noOpBuf.data()), BUF_SIZE);
  RingQueue<NoOpAfterMoveDestructor> noOpAfterMove(reinterpret_cast<NoOpAfterMoveDestructor*>(otherBuf.data()), BUF_SIZE);

  for (std::size_t i = 0; i < RUN_NUM; i++) {
    noOpAfterMoveRun(noOpAfterMove);
    noOpRun(noOp);
  }
  
  auto noOpStats = benchpp::Stats<benchpp::TimeCount_t>::generate(noOpTimer.times());
  auto noOpAfterMoveStats = benchpp::Stats<benchpp::TimeCount_t>::generate(noOpAfterMoveTimer.times());
  
  double combinedStddevOfMean = std::sqrt(noOpStats.varianceOfMean() + noOpAfterMoveStats.varianceOfMean());

  REQUIRE_THAT(noOpStats.mean, Catch::Matchers::WithinAbs(noOpAfterMoveStats.mean, combinedStddevOfMean * TOLERANCE));
  
}

}

}
