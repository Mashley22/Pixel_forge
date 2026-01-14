module;

#ifndef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
#include <exception>
#endif

#include <source_location>
#include <string_view>

module PixelForge.core.require;

namespace pf {

void
require(const bool expr,
        const std::string_view msg,
        const std::source_location location) {

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  if (!expr) {
    throw RequireFail{.msg = msg,
                      .loc = location}
  }
#else
  (void)msg;
  (void)location;
  if(!expr) {
    std::terminate();
  }
#endif
}

}
