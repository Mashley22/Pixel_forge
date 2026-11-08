#include <PixelForgeValidationHelpers/helpers.hpp>
#include <catch2/catch_test_macros.hpp>

import PixelForge.core;
import PixelForge.logging.backends.fast;

using namespace pf::log::fast_backend;

namespace {

PF_TEST_CASE("Payload", "[log][backends][fast]") {
  Payload payload;
  payload.header.is_last = false;
  payload.header.size = 0;

  constexpr decltype(payload.header.size) some_stupid_number = 1'000;
  for (decltype(payload.header.size) i = 0; i < some_stupid_number; i++) {
    payload.header.size = i;
    const auto data = payload.data();
    REQUIRE(data.data() == pf::pointer_cast<char*>(&payload + 1));
    REQUIRE(data.size() == i);
    REQUIRE(payload.end() == data.data() + data.size());
    REQUIRE(&payload.next() == pf::pointer_cast<Payload*>(data.data() + data.size()));
  }
}

PF_TEST_CASE("Block", "[log][backends][fast]") {
  Block block;

  REQUIRE(block.max_remaining_payload_capacity() <= block_size - sizeof(Header));
}

}
