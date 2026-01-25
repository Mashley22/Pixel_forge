module;

#include <atomic>

#include <PixelForge/core/macros.hpp>

module PixelForge.core.exception;

import PixelForge.core.require;
import PixelForge.core.status;

namespace pf {

namespace {

void
M_excepting(void) {
  PF_REQUIRE(currentStatus().load() == Status::OK);
  auto expected = Status::OK;
  currentStatus().compare_exchange_strong(expected,
                                          Status::EXCEPT,
                                          std::memory_order_seq_cst,
                                          std::memory_order_seq_cst);
}

}

namespace priv {

ExceptionImpl::ExceptionImpl(void) PF_NOEXCEPT { M_excepting(); }

}

Exception::Exception(void) PF_NOEXCEPT {}

}
