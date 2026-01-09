module PixelForge.core.status;

import PixelForge.core.assert;

#include <atomic>
#include <cstddef>
#include <new>

namespace pf {

// ugh no anonymous structs ...
struct alignas(std::hardware_destructive_interference_size) PaddedStatus {
  std::atomic<Status> status;
  std::byte __padding[std::hardware_destructive_interference_size - sizeof(Status)];
};

namespace {

PaddedStatus M_paddedStatus{ .status = Status::OK };

}

Status
loadStatus(const std::memory_order mem_order) {
  M_paddedStatus.status.load(mem_order);
}

void
setStatus(const Status newStatus, const std::memory_order mem_order) {
  M_paddedStatus.status.store(newStatus);
}

}
