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
}

export [[nodiscard]]
Status
loadStatus(const std::memory_order mem_order = std::memory_order_relaxed) PF_NOEXCEPT;

export
void
setStatus(const Status newStatus, const std::memory_order mem_order = std::memory_order_relaxed) PF_NOEXCEPT;

}
