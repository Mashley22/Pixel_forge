module;

#include <source_location>
#include <string_view>

export module PixelForge.core.assert;

namespace pf {

#ifdef PIXELFORGE_ASSERT_THROW
export
class AssertFail {
  std::string_view msg;
  std::source_location loc;
};
#endif

export
void
assert(const bool expr,
      const std::string_view msg = {},
      const std::source_location location = std::source_location::current());

}
