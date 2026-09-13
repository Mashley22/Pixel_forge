#include <array>
#include <cstddef>
#include <cstdint>

#include <PixelForgeValidationHelpers/helpers.hpp>
#include <catch2/catch_test_macros.hpp>

import PixelForge.core;
import PixelForge.mem;

namespace pf::mem {
namespace {

constexpr std::size_t arenaSize = 64;
constexpr std::size_t alignment = 8;

ObjectStorage<std::byte>
storageFor(std::array<std::byte, arenaSize>& storage) {
  return Buffer::from(storage).asObjects<std::byte>(storage.size());
}

}

PF_TEST_CASE("alloc tracks bytes and reports exhaustion", "[mem][linearArena]") {
  alignas(std::max_align_t) std::array<std::byte, arenaSize> storage{};
  LinearArena arena{storageFor(storage)};

  REQUIRE(arena.capacity() == arenaSize);
  REQUIRE(arena.in_use() == 0);
  REQUIRE(arena.remaining() == arenaSize);

  constexpr std::size_t firstSize = 7;
  constexpr std::size_t secondSize = 5;
  std::byte* const first = arena.alloc(firstSize);
  std::byte* const second = arena.alloc(secondSize);
  (void) first;
  (void) second;

  REQUIRE(first == storage.data());
  REQUIRE(second == storage.data() + firstSize);
  REQUIRE(arena.in_use() == firstSize + secondSize);
  REQUIRE(arena.remaining() == arenaSize - firstSize - secondSize);

  REQUIRE_FALSE(arena.try_alloc(arena.remaining() + 1).has_value());
  REQUIRE(arena.in_use() == firstSize + secondSize);
  REQUIRE_THROWS_AS(arena.alloc(arena.remaining() + 1), LinearArena::OOMError);
}

PF_TEST_CASE("aligned allocation pads and rejects insufficient storage",
             "[mem][linearArena]") {
  {
    alignas(std::max_align_t) std::array<std::byte, arenaSize> storage{};
    LinearArena arena{storageFor(storage)};

    const auto aligned = arena.try_alloc(4, alignment);
    REQUIRE(aligned.has_value());
    std::byte* const alignedPtr = *aligned;
    REQUIRE(alignedPtr == storage.data());
    REQUIRE(reinterpret_cast<std::uintptr_t>(alignedPtr) % alignment == 0);
    REQUIRE(arena.in_use() == 4);
    REQUIRE(arena.remaining() == arenaSize - 4);
  }

  {
    constexpr std::size_t paddedArenaSize = 32;
    constexpr std::size_t prefixSize = 3;
    constexpr std::size_t alignedSize = 4;
    constexpr std::size_t expectedPadding = alignment - prefixSize % alignment;
    alignas(std::max_align_t) std::array<std::byte, paddedArenaSize> storage{};
    LinearArena arena{Buffer::from(storage).asObjects<std::byte>(storage.size())};

    REQUIRE(arena.alloc(prefixSize) == storage.data());
    const auto aligned = arena.try_alloc(alignedSize, alignment);
    REQUIRE(aligned.has_value());
    std::byte* const alignedPtr = *aligned;
    REQUIRE(alignedPtr == storage.data() + alignment);
    REQUIRE(reinterpret_cast<std::uintptr_t>(alignedPtr) % alignment == 0);
    REQUIRE(arena.in_use() == prefixSize + expectedPadding + alignedSize);
    REQUIRE(arena.remaining() ==
            paddedArenaSize - prefixSize - expectedPadding - alignedSize);
  }
}

}
