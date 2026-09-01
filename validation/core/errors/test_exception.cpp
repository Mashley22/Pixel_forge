#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

import PixelForge.core;

namespace pf {

TEST_CASE("string view message", "[core][errors]") {
  const Exception e{std::string_view{"hello world"}};

  REQUIRE(std::string_view{e.what()} == "hello world");
  REQUIRE(e.what()[std::string_view{e.what()}.size()] == '\0');
}

TEST_CASE("const char message", "[core][errors]") {
  const Exception e{"hello world"};

  REQUIRE(std::string_view{e.what()} == "hello world");
}

TEST_CASE("std exception message", "[core][errors]") {
  const std::runtime_error error{"std message"};
  const Exception e{error};

  REQUIRE(std::string_view{e.what()} == "std message");
}

TEST_CASE("empty message", "[core][errors]") {
  const Exception e{std::string_view{""}};

  REQUIRE(std::string_view{e.what()} == "");
}

TEST_CASE("long message is truncated", "[core][errors]") {
  const std::string longMessage(600, 'a');
  const Exception e{std::string_view{longMessage}};

  REQUIRE(std::string_view{e.what()}.size() == Exception::msgBufSize - 1);
  REQUIRE(std::string_view{e.what()} ==
          std::string_view{longMessage}.substr(0, Exception::msgBufSize - 1));
  REQUIRE(e.what()[Exception::msgBufSize - 1] == '\0');
}

TEST_CASE("embedded null truncates message", "[core][errors]") {
  constexpr std::string_view embedded{"ab\0cd", 5};
  const Exception e{embedded};

  REQUIRE(std::string_view{e.what()} == "ab");
}

TEST_CASE("copy constructor preserves message", "[core][errors]") {
  const Exception original{"copy me"};
  const Exception& copy{original};

  REQUIRE(std::string_view{copy.what()} == "copy me");
}

TEST_CASE("thrown and caught by type", "[core][errors]") {
  try {
    throw Exception{"boom"};
  } catch (const Exception& e) {
    REQUIRE(std::string_view{e.what()} == "boom");
  }
}

TEST_CASE("static message buffer is shared", "[core][errors]") {
  const Exception first{"first message"};
  const Exception second{"second message"};

  REQUIRE(std::string_view{first.what()} == "second message");
  REQUIRE(std::string_view{second.what()} == "second message");
}

}
