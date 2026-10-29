module;

#include <cstddef>

#include <PixelForge/core/macros.hpp>

export module PixelForge.logging:backend.fast.collector;

import PixelForge.core;
import PixelForge.mem;
import :backend.fast.types;

namespace pf::log::fast_backend {

export class Collector {

public:
  using size_type = std::size_t;

  static constexpr size_type block_count = PIXELFORGE_FAST_LOG_BACKEND_COLLECTOR_BLOCK_COUNT;
  using alloc_func = std::byte*(*)(size_type size, size_type alignment);
  using dealloc_func = void(*)(const Buffer&);
  
  Collector(const alloc_func& allocate, 
      const dealloc_func& deallocate) {
    PF_REQUIRE_ASSUME(g_arena.is_null() && g_deallocator == nullptr);  
    g_deallocator = deallocate;
    std::byte* buf = allocate(block_count * block_size, alignof(Payload));

    if (buf == nullptr) throw;

    g_buf = buf;

    const mem::ConcurrentPoolArena::CreateParams params = {
      .buffer = {
        .data = g_buf,
        .size = block_count * block_size
      },
      .block_size  = block_size,
      .block_count = block_count,
      .alignment   = alignof(Payload)
    };

    g_arena = mem::ConcurrentPoolArena(params);
  }

  PF_DELETE_COPY_CONSTRUCTOR(Collector);
  PF_DELETE_MOVE_CONSTRUCTOR(Collector);

  ~Collector() PF_NOEXCEPT {
    if (g_arena.is_null()) return;

    g_deallocator(Buffer{ .data = g_buf, .size = block_size * block_count });
  }

  Buffer allocate() PF_NOEXCEPT {
    return g_arena.allocate().buffer;
  }

  void deallocate(Buffer& buf) PF_NOEXCEPT {
    g_arena.deallocate(buf);
  }

private:
  inline static mem::ConcurrentPoolArena g_arena{};
  inline static std::byte * g_buf{nullptr};
  inline static dealloc_func g_deallocator{nullptr};

};

}
