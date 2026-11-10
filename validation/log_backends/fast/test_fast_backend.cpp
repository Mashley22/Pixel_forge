#include <PixelForgeValidationHelpers/helpers.hpp>
#include <catch2/catch_test_macros.hpp>

import PixelForge.core;
import PixelForge.logging;
import PixelForge.logging.backends.fast;

using namespace pf::log::fast_backend;

namespace {

PF_TEST_CASE("Payload", "[log][backends][fast]") {
  Payload payload;
  payload.is_last = false;
  payload.size = 0;

  constexpr decltype(payload.size) some_stupid_number = 1'000;
  for (decltype(payload.size) i = 0; i < some_stupid_number; i++) {
    payload.size = i;
    const auto data = payload.data();
    auto ptr = pf::pointer_cast<void*>(data.data() + data.size());
    std::size_t some_stupid_size = some_stupid_number;
    REQUIRE(data.data() == pf::pointer_cast<char*>(&payload + 1));
    REQUIRE(data.size() == i);
    REQUIRE(payload.end() == data.data() + data.size());
    REQUIRE(payload.next() ==
            pf::pointer_cast<Payload*>(
                std::align(alignof(Payload), sizeof(Payload), ptr, some_stupid_size)));
  }
}

PF_TEST_CASE("Block", "[log][backends][fast]") {
  std::array<char, block_size> buffer;
  std::memset(buffer.data(), 0, buffer.size());
  Block& block = *std::construct_at(pf::pointer_cast<Block*>(buffer.data()));
  block.next() = &block;

  REQUIRE(block.next() == &block);
  REQUIRE(block.max_remaining_payload_capacity() <= block_size - sizeof(Header));
  REQUIRE(block.first()->size == 0);
  REQUIRE(block.first()->is_last);

  constexpr decltype(Payload::Header::size) some_stupid_number = 10;
  std::string str = "a";
  for (decltype(Payload::Header::size) i = 0; i < some_stupid_number; i++) {
    pf::log::Record record;
    record.msg = str;
    REQUIRE(block.add_record(record) == str.size());
    str += "a";
  }

  const Payload* payload = block.first();
  for (decltype(Payload::Header::size) i = 0; i < some_stupid_number; i++) {
    REQUIRE(payload->size == i + 1);
    REQUIRE(payload->is_last == (i == some_stupid_number - 1));
    payload = payload->next_loc();
  }
}

}
