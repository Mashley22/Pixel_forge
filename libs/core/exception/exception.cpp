module;

#include <atomic>
#include <string>
#include <string_view>

#include <PixelForge/core/macros.hpp>

module PixelForge.core.exception;

import PixelForge.core.assert;
import PixelForge.core.status;

namespace pf {

namespace {

void
M_excepting(void) {
  assert(currentStatus().load() == Status::OK);
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

Exception::Exception(void) {}

Exception::Exception(const std::string_view msg) : m_msg(msg) {}

Exception::Exception(std::string&& msg) PF_NOEXCEPT : m_msg(msg) {}

}
