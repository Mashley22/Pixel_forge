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

// ugh no anonymous structs ...
struct alignas(std::hardware_destructive_interference_size) PaddedStatus {
  std::atomic<Status> status;
  std::byte __padding[std::hardware_destructive_interference_size - sizeof(Status)] = {};
};

static_assert(sizeof(PaddedStatus) == std::hardware_destructive_interference_size);

namespace {

PaddedStatus M_paddedStatus{ .status = Status::OK };

}

export [[nodiscard]] constexpr std::atomic<Status>&
currentStatus(void) PF_NOEXCEPT {
  return M_paddedStatus.status;
}

}
