module;

#include <source_location>
#include <string_view>

export module PixelForge.core.require;

namespace pf {

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
export
struct RequireFail {
  std::string_view msg;
  std::source_location loc;
};
#endif

export
void
require(const bool expr,
        const std::string_view msg = {},
        const std::source_location location = std::source_location::current());

}
