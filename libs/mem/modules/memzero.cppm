module;

#include <cstddef>

export module PixelForge.mem.memzero;

namespace pf {

namespace mem {

export
void 
memzero(void* dest, std::size_t count);

export 
void
memzero_explicit(void* dest, std::size_t count);

}

}
