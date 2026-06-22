module;

#include <atomic>
#include <new>

#include <PixelForge/core/macros.hpp>

module PixelForge.core.status;

namespace pf {

namespace {

// ugh no anonymous structs ...
struct alignas(std::hardware_destructive_interference_size) PaddedStatus {
  std::atomic<Status> status;
  std::byte __padding[std::hardware_destructive_interference_size - sizeof(Status)] = {};
};

PaddedStatus M_paddedStatus{ .status = Status::OK };

static_assert(sizeof(PaddedStatus) == std::hardware_destructive_interference_size);

}

[[nodiscard]] std::atomic<Status>&
currentStatus(void) PF_NOEXCEPT {
  return M_paddedStatus.status;
}

}
