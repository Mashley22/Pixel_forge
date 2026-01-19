#include <catch2/catch_test_macros.hpp>

#include <cstring>

#define SECRET_LEN 64
#define BUFFER_SIZE 256
#define SECRET_OFFSET 128

#ifndef BUILD_OPTS
#define BUILD_OPTS
#endif

namespace pf {

namespace mem {

extern
void
stupidDeadBufferFunction(char * buf, std::size_t offset, std::size_t len);

TEST_CASE( "memzero_explicit_explicit " BUILD_OPTS , "[mem][basic][optimise][hacky]" ) {
  // stupid idea
  SECTION( "stupid idea" ) {
    char buf[BUFFER_SIZE];

    std::strcpy(buf + SECRET_OFFSET, "BIG SECRET PASSWORD123"); // I dont think it can ignore this anymore

    stupidDeadBufferFunction(buf, SECRET_OFFSET, SECRET_LEN);

    for (std::size_t i = 0; i < SECRET_LEN; i++) {
      REQUIRE(buf[i + SECRET_OFFSET] == 0x00);
    }
  }
}

}

}
