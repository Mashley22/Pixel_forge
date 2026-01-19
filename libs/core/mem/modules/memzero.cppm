module;

#include <cstddef>

export module PixelForge.core.mem.memzero;

import PixelForge.core.meta.enumBits;

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
