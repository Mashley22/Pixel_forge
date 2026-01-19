import PixelForge.core.mem.memzero;

#include <catch2/catch_test_macros.hpp>

#include <cstring>

#define SECRET_LEN 64
#define DUMMY_BYTE 0xAA

#ifndef BUILD_OPTS
#define BUILD_OPTS
#endif

namespace pf {

namespace mem {

TEST_CASE( "memzero_explicit_explicit " BUILD_OPTS , "[mem][basic][optimise][hacky]" ) {

  // I couldnt get it to work properly with a dead variable :( well just have to see
  SECTION( "see how explicit this really is" ) {
    unsigned char secret[SECRET_LEN]; 
    std::strcpy((char*)secret, "BIG SECRET PASSWORD123");
    
    memzero_explicit(secret, SECRET_LEN);

    for (std::size_t i = 0; i < SECRET_LEN; i++) {
      REQUIRE(secret[i] == 0x00);
    }
  }
}

}

}
