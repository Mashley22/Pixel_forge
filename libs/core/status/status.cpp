module;

#include <atomic>
#include <cstddef>
#include <new>

module PixelForge.core.status;

import PixelForge.core.assert;

namespace pf {

// ugh no anonymous structs ...
struct alignas(std::hardware_destructive_interference_size) PaddedStatus {
  std::atomic<Status> status;
  std::byte __padding[std::hardware_destructive_interference_size - sizeof(Status)];
};

static_assert(sizeof(PaddedStatus) == std::hardware_destructive_interference_size);

namespace {

PaddedStatus M_paddedStatus{ .status = Status::OK };

}

std::atomic<Status>&
loadStatus(void) {
  return M_paddedStatus.status;
}

}
