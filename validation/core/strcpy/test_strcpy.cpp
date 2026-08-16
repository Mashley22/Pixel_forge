#include <string>
#include <span>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

import PixelForge.core;

namespace pf {

namespace {

// ============================================================================
// strcpy Tests (string_view overload)
// ============================================================================

TEST_CASE("strcpy: Basic functionality", "[core][utils][strcpy]") {
  SECTION("Copy simple string") {
    std::string_view src = "hello";
    char dest[10] = {};
    
    auto result = strcpy(
      std::span<char>{dest, 10},
      src
    );
    
    REQUIRE(result > 0);
    REQUIRE(std::string(dest) == "hello");
    REQUIRE(dest[5] == '\0');
  }

  SECTION("Copy from string_view") {
    std::string_view src = "world";
    char dest[10] = {};
    
    auto result = strcpy(
      std::span<char>{dest, 10},
      src
    );
    
    REQUIRE(result > 0);
    REQUIRE(std::string(dest) == "world");
    REQUIRE(dest[5] == '\0');
  }

  SECTION("Copy null-terminated string") {
    std::string_view src = "test";
    char dest[10] = {};
    
    auto result = strcpy(
      std::span<char>{dest, 10},
      src
    );
    
    REQUIRE(result > 0);
    REQUIRE(std::string(dest) == "test");
    REQUIRE(dest[4] == '\0');
  }
}

TEST_CASE("strcpy: Buffer overflow protection", "[core][utils][strcpy]") {
  SECTION("Truncate when source exceeds destination") {
    std::string_view src = "hello world this is long";
    char dest[6] = {'X', 'X', 'X', 'X', 'X', 'X'};
    
    auto result = strcpy(
      std::span<char>{dest, 6},
      src
    );
    
    REQUIRE(result > 0);
    // Buffer should be: [h][e][l][l][o][\0]
    REQUIRE(dest[0] == 'h');
    REQUIRE(dest[1] == 'e');
    REQUIRE(dest[2] == 'l');
    REQUIRE(dest[3] == 'l');
    REQUIRE(dest[4] == 'o');
    REQUIRE(dest[5] == '\0');
  }

  SECTION("Guaranteed null termination at buffer end") {
    std::string_view src = "verylongstring";
    char dest[5] = {'A', 'A', 'A', 'A', 'A'};
    
    strcpy(
      std::span<char>{dest, 5},
      src
    );
    
    REQUIRE(dest[4] == '\0');
  }

  SECTION("Minimal buffer (size 1)") {
    std::string_view src = "hello";
    char dest[1] = {'X'};
    
    strcpy(
      std::span<char>{dest, 1},
      src
    );
    
    REQUIRE(dest[0] == '\0');
  }
}

TEST_CASE("strcpy: Empty source", "[core][utils][strcpy]") {
  SECTION("Empty string_view") {
    char dest[10] = {'X'};
    std::string_view empty_view = "";
    
    REQUIRE(strcpy(
      std::span<char>{dest, 10},
      empty_view
    ) == 1);
    
    REQUIRE(dest[0] == '\0');
  }

  SECTION("Empty string_view with small buffer") {
    char dest[5] = {'X', 'X', 'X', 'X', 'X'};
    std::string_view empty_view = "";
    
    auto result = strcpy(
      std::span<char>{dest, 5},
      empty_view
    );
    
    REQUIRE(result == 1);
    REQUIRE(dest[0] == '\0');
  }
}

TEST_CASE("strcpy: maxNullTerminators parameter", "[core][utils][strcpy]") {
  SECTION("With maxNullTerminators=0 (default)") {
    std::string_view src = "hello";
    char dest[20] = {};
    
    auto result = strcpy(
      std::span<char>{dest, 20},
      src,
      0
    );
    
    REQUIRE(result > 0);
    REQUIRE(std::string(dest) == "hello");
  }

  SECTION("With maxNullTerminators > 0") {
    std::string_view src = "test";
    char dest[10] = {};
    
    auto result = strcpy(
      std::span<char>{dest, 10},
      src,
      1
    );
    
    REQUIRE(result > 0);
    REQUIRE(std::string(dest) == "test");
  }
}

TEST_CASE("strcpy: Content preservation", "[core][utils][strcpy]") {
  SECTION("Special characters") {
    std::string_view src = "!@#$%^&*()";
    char dest[15] = {};
    
    strcpy(
      std::span<char>{dest, 15},
      src
    );
    
    REQUIRE(std::string(dest) == "!@#$%^&*()");
  }

  SECTION("Digits") {
    std::string_view src = "0123456789";
    char dest[15] = {};
    
    strcpy(
      std::span<char>{dest, 15},
      src
    );
    
    REQUIRE(std::string(dest) == "0123456789");
  }

  SECTION("Mixed case") {
    std::string_view src = "HeLLo WoRLd";
    char dest[15] = {};
    
    strcpy(
      std::span<char>{dest, 15},
      src
    );
    
    REQUIRE(std::string(dest) == "HeLLo WoRLd");
  }

  SECTION("Spaces and tabs") {
    std::string_view src = "hello  \t  world";
    char dest[20] = {};
    
    strcpy(
      std::span<char>{dest, 20},
      src
    );
    
    REQUIRE(std::string(dest) == "hello  \t  world");
  }
}

TEST_CASE("strcpy: Return value correctness", "[core][utils][strcpy]") {
  SECTION("Return value includes null terminator in count") {
    std::string_view src = "hi";
    char dest[10] = {};
    
    auto result = strcpy(
      std::span<char>{dest, 10},
      src
    );
    
    // Should return count including the null terminator
    REQUIRE(result > 2);  // At least the string + null
  }

  SECTION("Return value with truncation") {
    std::string_view src = "hello";
    char dest[3] = {};
    
    auto result = strcpy(
      std::span<char>{dest, 3},
      src
    );
    
    REQUIRE(result > 0);
  }

  SECTION("Return value is positive") {
    std::string_view src = "test";
    char dest[10] = {};
    
    auto result = strcpy(
      std::span<char>{dest, 10},
      src
    );
    
    REQUIRE(result > 0);
  }
}

TEST_CASE("strcpy: Large buffers", "[core][utils][strcpy]") {
  SECTION("Copy into large buffer") {
    std::string_view src = "test";
    std::vector<char> dest(1000, 'X');
    
    strcpy(
      std::span<char>{dest.data(), dest.size()},
      src
    );
    
    REQUIRE(std::string(dest.data()) == "test");
    REQUIRE(dest[4] == '\0');
  }

  SECTION("Copy large string into buffer") {
    std::string large_src(500, 'a');
    std::vector<char> dest(1000, 'X');
    
    strcpy(
      std::span<char>{dest.data(), dest.size()},
      large_src
    );

    for (std::size_t i = 0; i < 500; ++i) {
      REQUIRE(dest[i] == 'a');
    }
    REQUIRE(dest[500] == '\0');
  }

  SECTION("Copy exact large string") {
    std::string large_src(99, 'b');
    std::vector<char> dest(100, 'X');
    
    strcpy(
      std::span<char>{dest.data(), dest.size()},
      large_src
    );
    
    for (std::size_t i = 0; i < 99; ++i) {
      REQUIRE(dest[i] == 'b');
    }
    REQUIRE(dest[99] == '\0');
  }
}

TEST_CASE("strcpy: Boundary conditions", "[core][utils][strcpy]") {
  SECTION("Exact fit (buffer_size = string length + 1)") {
    std::string_view src = "fit";
    char dest[4] = {'X', 'X', 'X', 'X'};
    
    strcpy(
      std::span<char>{dest, 4},
      src
    );
    
    REQUIRE(dest[0] == 'f');
    REQUIRE(dest[1] == 'i');
    REQUIRE(dest[2] == 't');
    REQUIRE(dest[3] == '\0');
  }

  SECTION("One byte too small for full string") {
    std::string_view src = "hello";
    char dest[5] = {'X', 'X', 'X', 'X', 'X'};
    
    strcpy(
      std::span<char>{dest, 5},
      src
    );
    
    REQUIRE(dest[4] == '\0');
    REQUIRE(dest[0] == 'h');
    REQUIRE(dest[1] == 'e');
    REQUIRE(dest[2] == 'l');
    REQUIRE(dest[3] == 'l');
  }

  SECTION("Minimal buffer size") {
    std::string_view src = "x";
    char dest[2] = {'X', 'X'};
    
    strcpy(
      std::span<char>{dest, 2},
      src
    );
    
    REQUIRE(dest[0] == 'x');
    REQUIRE(dest[1] == '\0');
  }
}

TEST_CASE("strcpy: Multiple consecutive calls", "[core][utils][strcpy]") {
  SECTION("Overwrite previous content") {
    char dest[10] = {};
    
    strcpy(
      std::span<char>{dest, 10},
      std::string_view("first")
    );
    REQUIRE(std::string(dest) == "first");
    
    strcpy(
      std::span<char>{dest, 10},
      std::string_view("second")
    );
    REQUIRE(std::string(dest) == "second");
  }

  SECTION("Sequential writes to different buffers") {
    char dest1[10] = {};
    char dest2[10] = {};
    
    strcpy(
      std::span<char>{dest1, 10},
      std::string_view("alpha")
    );
    strcpy(
      std::span<char>{dest2, 10},
      std::string_view("beta")
    );
    
    REQUIRE(std::string(dest1) == "alpha");
    REQUIRE(std::string(dest2) == "beta");
  }
}

TEST_CASE("strcpy: Various string content", "[core][utils][strcpy]") {
  SECTION("Single character") {
    std::string_view src = "a";
    char dest[5] = {};
    
    strcpy(
      std::span<char>{dest, 5},
      src
    );
    
    REQUIRE(dest[0] == 'a');
    REQUIRE(dest[1] == '\0');
  }

  SECTION("Numbers as string") {
    std::string_view src = "12345";
    char dest[10] = {};
    
    strcpy(
      std::span<char>{dest, 10},
      src
    );
    
    REQUIRE(std::string(dest) == "12345");
  }

  SECTION("Path-like string") {
    std::string_view src = "/home/user/file.txt";
    char dest[30] = {};
    
    strcpy(
      std::span<char>{dest, 30},
      src
    );
    
    REQUIRE(std::string(dest) == "/home/user/file.txt");
  }

  SECTION("URL-like string") {
    std::string_view src = "https://example.com/path";
    char dest[50] = {};
    
    strcpy(
      std::span<char>{dest, 50},
      src
    );
    
    REQUIRE(std::string(dest) == "https://example.com/path");
  }
}

TEST_CASE("strcpy: Dest buffer exactly filled", "[core][utils][strcpy]") {
  SECTION("Fill entire buffer except last byte") {
    std::string_view src = "abcd";
    char dest[5] = {'X', 'X', 'X', 'X', 'X'};
    
    strcpy(
      std::span<char>{dest, 5},
      src
    );
    
    REQUIRE(dest[0] == 'a');
    REQUIRE(dest[1] == 'b');
    REQUIRE(dest[2] == 'c');
    REQUIRE(dest[3] == 'd');
    REQUIRE(dest[4] == '\0');
  }
}

}  // namespace

}  // namespace pf
