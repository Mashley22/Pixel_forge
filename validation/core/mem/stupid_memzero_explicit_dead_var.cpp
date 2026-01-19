import PixelForge.core.mem.memzero;

#include <cstddef>

namespace pf {

namespace mem {

__attribute__((noinline))
void
stupidDeadBufferFunction(char * buf, std::size_t offset, std::size_t len) {
  char * secret = buf + offset; 
  
  memzero_explicit(secret, len);
}

}

}
