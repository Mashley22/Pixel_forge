module;

#include <atomic>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.exception;

import PixelForge.core.require;
import PixelForge.core.status;

namespace pf {

namespace priv {

constexpr void
M_excepting(void) {
  PF_REQUIRE(currentStatus().load() == Status::OK);
  auto expected = Status::OK;
  currentStatus().compare_exchange_strong(expected,
                                          Status::EXCEPT,
                                          std::memory_order_seq_cst,
                                          std::memory_order_seq_cst);
}

class M_ExceptionImpl {
public:
  constexpr M_ExceptionImpl(void) PF_NOEXCEPT {
    M_excepting();
  }
};

}

export
class Exception : private priv::M_ExceptionImpl {
public:
  constexpr Exception(void) PF_NOEXCEPT;
};

}
