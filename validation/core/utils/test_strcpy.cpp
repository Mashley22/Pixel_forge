#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

import PixelForge.core;

namespace pf {

namespace {

// ============================================================================
// strcpy Tests (string_view overload)
// ============================================================================

// strcpy(span dest, string_view src, maxNullTerminators) returns the number of
// characters written to dest, including the guaranteed trailing null
// terminator. With no embedded nulls in src and a buffer of size D it copies
// min(D - 1, src.size()) characters, so the return value is
// min(D, src.size() + 1).

constexpr bool strcpyBasicConstexpr() {
  char dest[8] = {};
  const std::size_t result = strcpy(std::span<char>{dest, 8}, std::string_view{"hi"});
  return result == 3 && dest[0] == 'h' && dest[1] == 'i' && dest[2] == '\0';
}
static_assert(strcpyBasicConstexpr());

constexpr bool strcpyTruncationConstexpr() {
  char dest[4] = {'X', 'X', 'X', 'X'};
  const std::size_t result = strcpy(std::span<char>{dest, 4}, std::string_view{"abcdef"});
  return result == 4 && dest[0] == 'a' && dest[1] == 'b' && dest[2] == 'c' && dest[3] == '\0';
}
static_assert(strcpyTruncationConstexpr());

constexpr bool strcpyEmbeddedNullConstexpr() {
  char dest[16] = {};
  const std::string_view src{"he\0llo", 6};
  const std::size_t result = strcpy(std::span<char>{dest, 16}, src);
  return result == 3 && dest[0] == 'h' && dest[1] == 'e' && dest[2] == '\0';
}
static_assert(strcpyEmbeddedNullConstexpr());

TEST_CASE("strcpy: Basic functionality", "[core][utils][strcpy]") {
  SECTION("Copy simple string") {
    std::string_view src = "hello";
    char dest[10] = {};

    auto result = strcpy(
      std::span<char>{dest, 10},
      src
    );

    REQUIRE(result == 6);
    REQUIRE(std::string(dest) == "hello");
    REQUIRE(dest[5] == '\0');
    REQUIRE(dest[6] == '\0');
  }

  SECTION("Copy from string_view") {
    std::string_view src = "world";
    char dest[10] = {};

    auto result = strcpy(
      std::span<char>{dest, 10},
      src
    );

    REQUIRE(result == 6);
    REQUIRE(std::string(dest) == "world");
    REQUIRE(dest[5] == '\0');
  }

  SECTION("Copy from const char literal") {
    std::string_view src = "test";
    char dest[10] = {};

    auto result = strcpy(
      std::span<char>{dest, 10},
      src
    );

    REQUIRE(result == 5);
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

    REQUIRE(result == 6);
    REQUIRE(std::string(dest) == "hello");
    REQUIRE(dest[5] == '\0');
  }

  SECTION("Guaranteed null termination at buffer end") {
    std::string_view src = "verylongstring";
    char dest[5] = {'A', 'A', 'A', 'A', 'A'};

    auto result = strcpy(
      std::span<char>{dest, 5},
      src
    );

    REQUIRE(result == 5);
    REQUIRE(std::string(dest) == "very");
    REQUIRE(dest[4] == '\0');
  }

  SECTION("Minimal buffer (size 1)") {
    std::string_view src = "hello";
    char dest[1] = {'X'};

    auto result = strcpy(
      std::span<char>{dest, 1},
      src
    );

    REQUIRE(result == 1);
    REQUIRE(std::string(dest) == "");
  }
}

TEST_CASE("strcpy: Empty source", "[core][utils][strcpy]") {
  SECTION("Empty string_view") {
    char dest[10] = {'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'};
    std::string_view empty_view = "";

    REQUIRE(strcpy(
      std::span<char>{dest, 10},
      empty_view
    ) == 1);

    REQUIRE(std::string(dest) == "");
    for (std::size_t i = 1; i < 10; ++i) {
      REQUIRE(dest[i] == 'X');
    }
  }

  SECTION("Empty string_view with small buffer") {
    char dest[5] = {'X', 'X', 'X', 'X', 'X'};
    std::string_view empty_view = "";

    auto result = strcpy(
      std::span<char>{dest, 5},
      empty_view
    );

    REQUIRE(result == 1);
    REQUIRE(std::string(dest) == "");
    for (std::size_t i = 1; i < 5; ++i) {
      REQUIRE(dest[i] == 'X');
    }
  }
}

TEST_CASE("strcpy: maxNullTerminators parameter", "[core][utils][strcpy]") {
  SECTION("Default (0) with no embedded nulls behaves like standard copy") {
    std::string_view src = "hello";
    char dest[20] = {};

    auto result = strcpy(
      std::span<char>{dest, 20},
      src
    );

    REQUIRE(result == 6);
    REQUIRE(std::string(dest) == "hello");
  }

  SECTION("maxNullTerminators > 0 with no embedded nulls") {
    std::string_view src = "test";
    char dest[10] = {};

    auto result = strcpy(
      std::span<char>{dest, 10},
      src,
      1
    );

    REQUIRE(result == 5);
    REQUIRE(std::string(dest) == "test");
  }

  SECTION("maxNullTerminators=0 stops at first embedded null") {
    const std::string_view src{"he\0llo", 6};
    char dest[10] = {'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'};

    auto result = strcpy(
      std::span<char>{dest, 10},
      src
    );

    REQUIRE(result == 3);
    REQUIRE(std::string(dest, result - 1) == std::string_view{"he", 2});
    REQUIRE(dest[result - 1] == '\0');
    REQUIRE(dest[3] == 'X');
  }

  SECTION("maxNullTerminators=1 copies the first embedded null") {
    const std::string_view src{"he\0llo", 6};
    char dest[10] = {'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'};

    auto result = strcpy(
      std::span<char>{dest, 10},
      src,
      1
    );

    REQUIRE(result == 7);
    REQUIRE(std::string(dest, result - 1) == std::string_view{"he\0llo", 6});
    REQUIRE(dest[result - 1] == '\0');
    REQUIRE(dest[7] == 'X');
  }

  SECTION("maxNullTerminators=2 copies all embedded nulls") {
    const std::string_view src{"he\0llo\0w", 8};
    char dest[12] = {'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'};

    auto result = strcpy(
      std::span<char>{dest, 12},
      src,
      2
    );

    REQUIRE(result == 9);
    REQUIRE(std::string(dest, result - 1) == std::string_view{"he\0llo\0w", 8});
    REQUIRE(dest[result - 1] == '\0');
    REQUIRE(dest[9] == 'X');
  }

  SECTION("More embedded nulls than maxNullTerminators truncates") {
    const std::string_view src{"a\0b\0c", 5};
    char dest[10] = {'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'};

    auto result = strcpy(
      std::span<char>{dest, 10},
      src,
      1
    );

    REQUIRE(result == 4);
    REQUIRE(std::string(dest, result - 1) == std::string_view{"a\0b", 3});
    REQUIRE(dest[result - 1] == '\0');
    REQUIRE(dest[4] == 'X');
  }
}

TEST_CASE("strcpy: Content preservation", "[core][utils][strcpy]") {
  SECTION("Special characters") {
    std::string_view src = "!@#$%^&*()";
    char dest[15] = {};

    auto result = strcpy(
      std::span<char>{dest, 15},
      src
    );

    REQUIRE(result == 11);
    REQUIRE(std::string(dest) == "!@#$%^&*()");
  }

  SECTION("Digits") {
    std::string_view src = "0123456789";
    char dest[15] = {};

    auto result = strcpy(
      std::span<char>{dest, 15},
      src
    );

    REQUIRE(result == 11);
    REQUIRE(std::string(dest) == "0123456789");
  }

  SECTION("Mixed case") {
    std::string_view src = "HeLLo WoRLd";
    char dest[15] = {};

    auto result = strcpy(
      std::span<char>{dest, 15},
      src
    );

    REQUIRE(result == 12);
    REQUIRE(std::string(dest) == "HeLLo WoRLd");
  }

  SECTION("Spaces and tabs") {
    std::string_view src = "hello  \t  world";
    char dest[20] = {};

    auto result = strcpy(
      std::span<char>{dest, 20},
      src
    );

    REQUIRE(result == 16);
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

    REQUIRE(result == 3);
  }

  SECTION("Return value with truncation") {
    std::string_view src = "hello";
    char dest[3] = {};

    auto result = strcpy(
      std::span<char>{dest, 3},
      src
    );

    REQUIRE(result == 3);
    REQUIRE(std::string(dest) == "he");
    REQUIRE(dest[2] == '\0');
  }

  SECTION("Return value equals string length plus one") {
    std::string_view src = "test";
    char dest[10] = {};

    auto result = strcpy(
      std::span<char>{dest, 10},
      src
    );

    REQUIRE(result == 5);
  }
}

TEST_CASE("strcpy: Large buffers", "[core][utils][strcpy]") {
  SECTION("Copy into large buffer") {
    std::string_view src = "test";
    std::vector<char> dest(1000, 'X');

    auto result = strcpy(
      std::span<char>{dest.data(), dest.size()},
      src
    );

    REQUIRE(result == 5);
    REQUIRE(std::string(dest.data()) == "test");
    REQUIRE(dest[4] == '\0');
    REQUIRE(dest[5] == 'X');
  }

  SECTION("Copy large string into buffer") {
    std::string large_src(500, 'a');
    std::vector<char> dest(1000, 'X');

    auto result = strcpy(
      std::span<char>{dest.data(), dest.size()},
      large_src
    );

    REQUIRE(result == 501);
    REQUIRE(std::string(dest.data(), result - 1) == large_src);
    REQUIRE(dest[500] == '\0');
    REQUIRE(dest[501] == 'X');
  }

  SECTION("Copy exact large string") {
    std::string large_src(99, 'b');
    std::vector<char> dest(100, 'X');

    auto result = strcpy(
      std::span<char>{dest.data(), dest.size()},
      large_src
    );

    REQUIRE(result == 100);
    REQUIRE(std::string(dest.data(), result - 1) == large_src);
    REQUIRE(dest[99] == '\0');
  }
}

TEST_CASE("strcpy: Boundary conditions", "[core][utils][strcpy]") {
  SECTION("Exact fit (buffer_size = string length + 1)") {
    std::string_view src = "fit";
    char dest[4] = {'X', 'X', 'X', 'X'};

    auto result = strcpy(
      std::span<char>{dest, 4},
      src
    );

    REQUIRE(result == 4);
    REQUIRE(std::string(dest) == "fit");
    REQUIRE(dest[3] == '\0');
  }

  SECTION("One byte too small for full string") {
    std::string_view src = "hello";
    char dest[5] = {'X', 'X', 'X', 'X', 'X'};

    auto result = strcpy(
      std::span<char>{dest, 5},
      src
    );

    REQUIRE(result == 5);
    REQUIRE(std::string(dest) == "hell");
    REQUIRE(dest[4] == '\0');
  }

  SECTION("Minimal buffer size") {
    std::string_view src = "x";
    char dest[2] = {'X', 'X'};

    auto result = strcpy(
      std::span<char>{dest, 2},
      src
    );

    REQUIRE(result == 2);
    REQUIRE(std::string(dest) == "x");
    REQUIRE(dest[1] == '\0');
  }

  SECTION("Buffer larger than source leaves tail untouched") {
    std::string_view src = "abc";
    char dest[8] = {'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'};

    auto result = strcpy(
      std::span<char>{dest, 8},
      src
    );

    REQUIRE(result == 4);
    REQUIRE(std::string(dest) == "abc");
    REQUIRE(dest[3] == '\0');
    for (std::size_t i = 4; i < 8; ++i) {
      REQUIRE(dest[i] == 'X');
    }
  }
}

TEST_CASE("strcpy: Multiple consecutive calls", "[core][utils][strcpy]") {
  SECTION("Overwrite previous content") {
    char dest[10] = {};

    auto result1 = strcpy(
      std::span<char>{dest, 10},
      std::string_view("first")
    );
    REQUIRE(result1 == 6);
    REQUIRE(std::string(dest) == "first");

    auto result2 = strcpy(
      std::span<char>{dest, 10},
      std::string_view("second")
    );
    REQUIRE(result2 == 7);
    REQUIRE(std::string(dest) == "second");
  }

  SECTION("Sequential writes to different buffers") {
    char dest1[10] = {};
    char dest2[10] = {};

    auto result1 = strcpy(
      std::span<char>{dest1, 10},
      std::string_view("alpha")
    );
    auto result2 = strcpy(
      std::span<char>{dest2, 10},
      std::string_view("beta")
    );

    REQUIRE(result1 == 6);
    REQUIRE(result2 == 5);
    REQUIRE(std::string(dest1) == "alpha");
    REQUIRE(std::string(dest2) == "beta");
  }
}

TEST_CASE("strcpy: Various string content", "[core][utils][strcpy]") {
  SECTION("Single character") {
    std::string_view src = "a";
    char dest[5] = {};

    auto result = strcpy(
      std::span<char>{dest, 5},
      src
    );

    REQUIRE(result == 2);
    REQUIRE(std::string(dest) == "a");
    REQUIRE(dest[1] == '\0');
  }

  SECTION("Numbers as string") {
    std::string_view src = "12345";
    char dest[10] = {};

    auto result = strcpy(
      std::span<char>{dest, 10},
      src
    );

    REQUIRE(result == 6);
    REQUIRE(std::string(dest) == "12345");
  }

  SECTION("Path-like string") {
    std::string_view src = "/home/user/file.txt";
    char dest[30] = {};

    auto result = strcpy(
      std::span<char>{dest, 30},
      src
    );

    REQUIRE(result == 20);
    REQUIRE(std::string(dest) == "/home/user/file.txt");
  }

  SECTION("URL-like string") {
    std::string_view src = "https://example.com/path";
    char dest[50] = {};

    auto result = strcpy(
      std::span<char>{dest, 50},
      src
    );

    REQUIRE(result == 25);
    REQUIRE(std::string(dest) == "https://example.com/path");
  }
}

TEST_CASE("strcpy: Dest buffer exactly filled", "[core][utils][strcpy]") {
  SECTION("Fill entire buffer except last byte") {
    std::string_view src = "abcd";
    char dest[5] = {'X', 'X', 'X', 'X', 'X'};

    auto result = strcpy(
      std::span<char>{dest, 5},
      src
    );

    REQUIRE(result == 5);
    REQUIRE(std::string(dest) == "abcd");
    REQUIRE(dest[4] == '\0');
  }
}

TEST_CASE("strcpy: Sweep over buffer sizes", "[core][utils][strcpy]") {
  const std::string src_str = "abcdefgh";
  const std::string_view src = src_str;

  for (std::size_t bufSize = 1; bufSize <= 20; ++bufSize) {
    CAPTURE(bufSize);
    std::vector<char> dest(bufSize, 'X');

    const auto result = strcpy(std::span<char>{dest.data(), dest.size()}, src);

    const std::size_t expectedCopy = std::min(bufSize - 1, src.size());
    REQUIRE(result == expectedCopy + 1);

    REQUIRE(std::string(dest.data(), result - 1) == src.substr(0, result - 1));
    REQUIRE(dest[expectedCopy] == '\0');
    for (std::size_t i = expectedCopy + 1; i < bufSize; ++i) {
      REQUIRE(dest[i] == 'X');
    }
  }
}

// ============================================================================
// copy_until Tests (building block used by strcpy)
// ============================================================================

TEST_CASE("copy_until: Basic functionality", "[core][utils][strcpy]") {
  SECTION("Copies all elements when nothing is a terminator") {
    const std::string_view src = "hello";
    char dest[10] = {};

    auto result = copy_until(
      std::span<char>{dest, 10},
      std::span<const char>{src.data(), src.size()},
      [](char val) { return val == '\0'; }
    );

    REQUIRE(result == 5);
    REQUIRE(std::string(dest) == "hello");
  }

  SECTION("Stops before first terminator by default") {
    const std::string_view src{"he\0lo", 5};
    char dest[10] = {'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'};

    auto result = copy_until(
      std::span<char>{dest, 10},
      std::span<const char>{src.data(), src.size()},
      [](char val) { return val == '\0'; }
    );

    REQUIRE(result == 2);
    REQUIRE(std::string(dest, result) == "he");
    REQUIRE(dest[result] == 'X');
  }

  SECTION("maxTerminators allows copying terminators") {
    const std::string_view src{"he\0lo", 5};
    char dest[10] = {'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'};

    auto result = copy_until(
      std::span<char>{dest, 10},
      std::span<const char>{src.data(), src.size()},
      [](char val) { return val == '\0'; },
      1
    );

    REQUIRE(result == 5);
    REQUIRE(std::string(dest, result) == std::string_view{"he\0lo", 5});
  }

  SECTION("Respects dest size limit") {
    const std::string_view src = "hello";
    char dest[3] = {'X', 'X', 'X'};

    auto result = copy_until(
      std::span<char>{dest, 3},
      std::span<const char>{src.data(), src.size()},
      [](char val) { return val == '\0'; }
    );

    REQUIRE(result == 3);
    REQUIRE(std::string(dest, result) == "hel");
  }

  SECTION("Empty dest") {
    const std::string_view src = "hello";
    char dest[1] = {'X'};

    auto result = copy_until(
      std::span<char>{dest, 0},
      std::span<const char>{src.data(), src.size()},
      [](char val) { return val == '\0'; }
    );

    REQUIRE(result == 0);
    REQUIRE(dest[0] == 'X');
  }

  SECTION("Terminator as first element") {
    const std::string_view src{"\0abc", 4};
    char dest[10] = {'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'};

    auto result = copy_until(
      std::span<char>{dest, 10},
      std::span<const char>{src.data(), src.size()},
      [](char val) { return val == '\0'; }
    );

    REQUIRE(result == 0);
    REQUIRE(dest[0] == 'X');
  }
}

}  // namespace

}  // namespace pf
