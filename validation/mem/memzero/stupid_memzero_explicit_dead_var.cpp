import PixelForge.mem.memzero;

#include <cstddef>

namespace pf {

namespace mem {

// trying to make dead_var as lifetimed out as possible
__attribute__((noinline))
void
stupidDeadBufferFunction(char * buf, std::size_t offset, std::size_t len) {
  char * dead_var = buf + offset; 
  
  memzero_explicit(dead_var, len);
}

}

}
