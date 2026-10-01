#include <array>
#include <cstddef>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include <PixelForgeValidationHelpers/helpers.hpp>

import PixelForge.core;

namespace pf {

PF_TEST_CASE("Construction", "[core][Buffer]") {
  Buffer buf{};
  REQUIRE(buf.data == nullptr);
  REQUIRE(buf.size == 0);

  std::array<std::byte, 64> storage{};
  Buffer buf16{storage.data(), storage.size()};
  REQUIRE(buf16.data == storage.data());
  REQUIRE(buf16.size == storage.size());
}

PF_TEST_CASE("as_objects", "[core][Buffer][ObjectStorage]") {
  alignas(std::max_align_t) std::array<std::byte, 64> storage{};
  Buffer buf{.data = storage.data(), .size = storage.size()};

  SECTION("success") {
    constexpr std::size_t valid_obj_count = 10;
    static_assert(valid_obj_count * sizeof(int) < storage.size());
    ObjectStorage<int> objs = buf.as_objects<int>(valid_obj_count);
    REQUIRE(objs.size == valid_obj_count);
    REQUIRE(objs.data == reinterpret_cast<int*>(storage.data()));
  }

  SECTION("alignment error") {
    constexpr std::size_t misalignAmount = 5;
    Buffer misalignedBuf{.data = storage.data() + misalignAmount,
                         .size = storage.size() - misalignAmount};

    REQUIRE_THROWS_AS(misalignedBuf.as_objects<long long>(1), Buffer::AlignmentError);

    try {
      [[maybe_unused]] auto _ = misalignedBuf.as_objects<long long>(1);
      REQUIRE(false);
    } catch (const Buffer::AlignmentError& e) {
      REQUIRE(e.required_alignment == alignof(long long));
      REQUIRE(e.ptr_val == reinterpret_cast<std::uintptr_t>(misalignedBuf.data));
    }
  }

  SECTION("size error") {
    constexpr std::size_t tooManyObjs = 10000;
    static_assert(tooManyObjs * sizeof(int) > storage.size());
    REQUIRE_THROWS_AS(buf.as_objects<int>(tooManyObjs), Buffer::SizeError);

    try {
      [[maybe_unused]] auto _ = buf.as_objects<int>(tooManyObjs);
      REQUIRE(false);
    } catch (const Buffer::SizeError& e) {
      REQUIRE(e.buffer_size == storage.size());
      REQUIRE(e.num_objects == tooManyObjs);
      REQUIRE(e.object_size == sizeof(int));
    }
  }
}

PF_TEST_CASE("as_objects_unchecked", "[core][Buffer][ObjectStorage]") {
  alignas(std::max_align_t) std::array<std::byte, 64> storage{};
  Buffer buf{.data = storage.data(), .size = storage.size()};

  SECTION("success") {
    constexpr std::size_t valid_obj_count = 10;
    static_assert(valid_obj_count * sizeof(int) < storage.size());
    ObjectStorage<int> objs = buf.as_objects_unchecked<int>(valid_obj_count);
    REQUIRE(objs.size == valid_obj_count);
    REQUIRE(objs.data == reinterpret_cast<int*>(storage.data()));
  }

  SECTION("buffer too small") {
    constexpr std::size_t tooManyObjs = 1000;
    static_assert(tooManyObjs * sizeof(int) > storage.size());
#ifndef NDEBUG
    REQUIRE_PF_REQUIRE_FAIL(buf.as_objects_unchecked<int>(tooManyObjs));
#else
    auto obj_storage = buf.as_objects_unchecked<int>(tooManyObjs);
    REQUIRE(obj_storage.data == buf.data);
    REQUIRE(obj_storage.size == tooManyObjs);
#endif
  }

  SECTION("misaligned buffer") {
    constexpr std::size_t misalignBy = 3;
    constexpr std::size_t smallObjNum = 1;
    static_assert(smallObjNum * sizeof(int) < storage.size());

    Buffer misalignedBuf{storage.data() + misalignBy, storage.size() - misalignBy};

#ifndef NDEBUG
    REQUIRE_PF_REQUIRE_FAIL(misalignedBuf.as_objects_unchecked<int>(smallObjNum));
#else
    auto obj_storage = misalignedBuf.as_objects_unchecked<int>(smallObjNum);
    REQUIRE(obj_storage.data == misalignedBuf.data);
    REQUIRE(obj_storage.size == smallObjNum);
#endif
  }
}

PF_TEST_CASE("try_as_objects", "[core][Buffer][ObjectStorage]") {
  alignas(std::max_align_t) std::array<std::byte, 64> storage{};
  Buffer buf{.data = storage.data(), .size = storage.size()};

  SECTION("success") {
    constexpr std::size_t validObjCount = 10;
    static_assert(validObjCount * sizeof(int) < storage.size());

    std::optional<ObjectStorage<int>> result = buf.try_as_objects<int>(validObjCount);
    REQUIRE(result.has_value());
    REQUIRE(result.value().size == validObjCount);
    REQUIRE(result.value().data == storage.data());
  }

  SECTION("misaligned") {
    constexpr std::size_t misalignOffset = 1;
    Buffer misAlignedBuf{storage.data() + misalignOffset,
                         storage.size() - misalignOffset};
    REQUIRE(!misAlignedBuf.try_as_objects<int>(1).has_value());
  }

  SECTION("buffer too small") {
    constexpr std::size_t tooManyObjects = 1000;
    static_assert(tooManyObjects * sizeof(int) > storage.size());
    REQUIRE(!buf.try_as_objects<int>(tooManyObjects).has_value());
  }
}

}
