module PixelForge.core.exception;

import PixelForge.core.status;

#include <atomic>

namespace pf {

namespace {

void
M_excepting(void) {
  currentStatus().compare_exchange_strong(Status::OK,
                                          Status::EXCEPT,
                                          std::memory_order_seq_cst,
                                          std::memory_order_seq_cst);
}

}

namespace priv {

ExceptionImpl::ExceptionImpl(void) { M_excepting(); }

}

Exception(void) : m_msg{} {}

Exception(const std::string_view msg) : m_msg(msg) {}

Exception(std::string&& msg) : m_msg(msg) {}

}
