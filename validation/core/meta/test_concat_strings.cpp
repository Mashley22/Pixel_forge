import PixelForge.core.meta;

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_case_sensitive.hpp>

#include <string_view>

#define HELLO "hello"
#define SPACE " "
#define WORLD "world"
#define BANG "!"

#define HELLO_WORLD HELLO SPACE WORLD BANG

namespace {

constexpr std::string_view hello = HELLO;
constexpr std::string_view space = SPACE;
constexpr std::string_view world = WORLD;
constexpr std::string_view bang = BANG;
constexpr std::string_view hello_world = HELLO_WORLD;

}

namespace pf {

namespace meta {

using HelloWorldConcat = ConcatStrings<hello, space, world, bang>;

TEST_CASE( "concatStrings", "[core][meta]" ) {
  STATIC_REQUIRE( HelloWorldConcat::sv() == HELLO_WORLD);

  for (std::size_t i = 0; i < hello_world.size(); i++) {
    REQUIRE( HelloWorldConcat::c_str()[i] == hello_world[i]);
    REQUIRE( HelloWorldConcat::arr[i] == hello_world[i]);
  }

  STATIC_REQUIRE( HelloWorldConcat::c_str()[hello_world.size()] == '\0');
  STATIC_REQUIRE( HelloWorldConcat::arr[hello_world.size()] == '\0');
}

}

}
