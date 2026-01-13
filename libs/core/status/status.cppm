module;

#include <atomic>
#include <cstdint>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.status;

namespace pf {

using StatusUnderlying_t = std::uint64_t;

export
enum Status : StatusUnderlying_t {
  OK,
  TERMINATE,
  EXCEPT
};

export [[nodiscard]]
std::atomic<Status>&
currentStatus(void) PF_NOEXCEPT;

}
