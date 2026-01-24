module;

#include <cstddef>
#include <cstring>

module PixelForge.mem.memzero;

import PixelForge.core.require;

namespace pf {

namespace mem {

void
memzero(void* dest, std::size_t count) {
  require(dest != nullptr);
  require(count != 0);

  std::memset(dest, 0, count);
}

void
memzero_explicit(void* dest, std::size_t count) {
  require(dest != nullptr);
  require(count != 0);
  
  volatile std::byte* p = static_cast<volatile std::byte*>(dest);

  for (std::size_t i = 0; i < count; i++) {
    p[i] = std::byte{0};
  }

}

}

}
