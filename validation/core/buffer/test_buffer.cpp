#include <array>
#include <cstddef>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include <PixelForgeValidationHelpers/helpers.hpp>

import PixelForge.core;

namespace pf {

PF_TEST_CASE("pointer_cast", "[core][Buffer]") {
  int val = 42;
  void* ptr = &val;

  int* int_ptr = pointer_cast<int*>(ptr);
  REQUIRE(int_ptr == &val);

  void* void_ptr = pointer_cast<void*>(int_ptr);
  REQUIRE(void_ptr == ptr);

  std::uintptr_t addr = pointer_cast<std::uintptr_t>(ptr);
  REQUIRE(addr == reinterpret_cast<std::uintptr_t>(ptr));

  int* back = pointer_cast<int*>(addr);
  REQUIRE(back == &val);

  void* null_ptr = nullptr;
  REQUIRE(pointer_cast<void*>(null_ptr) == nullptr);
  REQUIRE(pointer_cast<std::uintptr_t>(null_ptr) == 0);
}

PF_TEST_CASE("isAligned", "[core][Buffer]") {
  alignas(16) std::array<std::byte, 32> buf16{};
  alignas(8) std::array<std::byte, 32> buf8{};
  alignas(4) std::array<std::byte, 32> buf4{};

  REQUIRE(isAligned<std::max_align_t>(buf16.data()));
  REQUIRE(isAligned<int64_t>(buf16.data()));
  REQUIRE(isAligned<int>(buf16.data()));

  REQUIRE(isAligned<int64_t>(buf8.data()));

  REQUIRE(isAligned<int>(buf4.data()));

  REQUIRE(isAligned<int>(reinterpret_cast<char*>(buf4.data())));
  REQUIRE(isAligned<int>(reinterpret_cast<unsigned char*>(buf4.data())));
  REQUIRE(isAligned<int>(buf4.data()));
}

PF_TEST_CASE("Construction", "[core][Buffer]") {
  Buffer buf{};
  REQUIRE(buf.data == nullptr);
  REQUIRE(buf.size == 0);

  std::array<std::byte, 64> storage{};
  Buffer buf16{storage.data(), storage.size()};
  REQUIRE(buf16.data == storage.data());
  REQUIRE(buf16.size == storage.size());
}

PF_TEST_CASE("asObjects", "[core][Buffer][ObjectStorage]") {
  alignas(std::max_align_t) std::array<std::byte, 64> storage{};
  Buffer buf{.data = storage.data(), .size = storage.size()};

  SECTION("success") {
    constexpr std::size_t valid_obj_count = 10;
    static_assert(valid_obj_count * sizeof(int) < storage.size());
    ObjectStorage<int> objs = buf.asObjects<int>(valid_obj_count);
    REQUIRE(objs.size == valid_obj_count);
    REQUIRE(objs.data == reinterpret_cast<int*>(storage.data()));
  }

  SECTION("alignment error") {
    constexpr std::size_t misalignAmount = 5;
    Buffer misalignedBuf{.data = storage.data() + misalignAmount,
                         .size = storage.size() - misalignAmount};

    REQUIRE_THROWS_AS(misalignedBuf.asObjects<long long>(1), Buffer::AlignmentError);

    try {
      [[maybe_unused]] auto _ = misalignedBuf.asObjects<long long>(1);
      REQUIRE(false);
    } catch (const Buffer::AlignmentError& e) {
      REQUIRE(e.requiredAlignment == alignof(long long));
      REQUIRE(e.ptrVal == reinterpret_cast<std::uintptr_t>(misalignedBuf.data));
    }
  }

  SECTION("size error") {
    constexpr std::size_t tooManyObjs = 10000;
    static_assert(tooManyObjs * sizeof(int) > storage.size());
    REQUIRE_THROWS_AS(buf.asObjects<int>(tooManyObjs), Buffer::SizeError);

    try {
      [[maybe_unused]] auto _ = buf.asObjects<int>(tooManyObjs);
      REQUIRE(false);
    } catch (const Buffer::SizeError& e) {
      REQUIRE(e.bufferSize == storage.size());
      REQUIRE(e.numObjects == tooManyObjs);
      REQUIRE(e.objectSize == sizeof(int));
    }
  }
}

PF_TEST_CASE("asObjects_unchecked", "[core][Buffer][ObjectStorage]") {
  alignas(std::max_align_t) std::array<std::byte, 64> storage{};
  Buffer buf{.data = storage.data(), .size = storage.size()};

  SECTION("success") {
    constexpr std::size_t valid_obj_count = 10;
    static_assert(valid_obj_count * sizeof(int) < storage.size());
    ObjectStorage<int> objs = buf.asObjects_unchecked<int>(valid_obj_count);
    REQUIRE(objs.size == valid_obj_count);
    REQUIRE(objs.data == reinterpret_cast<int*>(storage.data()));
  }

  SECTION("buffer too small") {
    constexpr std::size_t tooManyObjs = 1000;
    static_assert(tooManyObjs * sizeof(int) > storage.size());
    REQUIRE_PF_REQUIRE_FAIL(buf.asObjects_unchecked<int>(tooManyObjs));
  }

  SECTION("misaligned buffer") {
    constexpr std::size_t misalignBy = 3;
    constexpr std::size_t smallObjNum = 1;
    static_assert(smallObjNum * sizeof(int) < storage.size());

    Buffer misalignedBuf{storage.data() + misalignBy, storage.size() - misalignBy};
    REQUIRE_PF_REQUIRE_FAIL(misalignedBuf.asObjects_unchecked<int>(smallObjNum));
  }
}

PF_TEST_CASE("try_asObjects", "[core][Buffer][ObjectStorage]") {
  alignas(std::max_align_t) std::array<std::byte, 64> storage{};
  Buffer buf{.data = storage.data(), .size = storage.size()};

  SECTION("success") {
    constexpr std::size_t validObjCount = 10;
    static_assert(validObjCount * sizeof(int) < storage.size());

    std::optional<ObjectStorage<int>> result = buf.try_asObjects<int>(validObjCount);
    REQUIRE(result.has_value());
    REQUIRE(result.value().size == validObjCount);
    REQUIRE(result.value().data == storage.data());
  }

  SECTION("misaligned") {
    constexpr std::size_t misalignOffset = 1;
    Buffer misAlignedBuf{storage.data() + misalignOffset,
                         storage.size() - misalignOffset};
    REQUIRE(!misAlignedBuf.try_asObjects<int>(1).has_value());
  }

  SECTION("buffer too small") {
    constexpr std::size_t tooManyObjects = 1000;
    static_assert(tooManyObjects * sizeof(int) > storage.size());
    REQUIRE(!buf.try_asObjects<int>(tooManyObjects).has_value());
  }
}

}
