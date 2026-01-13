module;

#ifndef PIXELFORGE_ASSERT_THROW
#include <exception>
#endif

#include <source_location>
#include <string_view>

module PixelForge.core.assert;

namespace pf {

namespace core {

void
assert(const bool expr,
      const std::string_view msg,
      const std::source_location location) {

#ifdef PIXELFORGE_ASSERT_THROW
  throw AssertFail{.msg = msg,
                   .loc = location}
#else
  std::terminate();
#endif
}

}

}
