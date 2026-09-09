#include <memory>
#include <vector>

#include <PixelForgeValidationHelpers/helpers.hpp>
#include <catch2/catch_test_macros.hpp>

import PixelForge.core;

namespace pf {

namespace {

struct TestStruct {
  int value = 42;
  constexpr int
  getValue() const {
    return value;
  }
  constexpr void
  setValue(int v) {
    value = v;
  }
};

constexpr bool
nonNullBasicConstexpr() {
  int x = 5;
  NonNull<int*> nn(&x);
  return *nn == 5 && static_cast<int*>(nn) == &x;
}
static_assert(nonNullBasicConstexpr());

constexpr bool
nonNullImplicitCast() {
  int x = 10;
  NonNull<int*> nn(&x);
  int* raw = nn; // implicit
  return raw == &x;
}
static_assert(nonNullImplicitCast());

constexpr bool
nonNullOperatorArrow() {
  TestStruct obj;
  NonNull<TestStruct*> nn(&obj);
  return nn->getValue() == 42;
}
static_assert(nonNullOperatorArrow());

} // namespace anon
  //
PF_TEST_CASE("pointer_cast", "[core][pointer]") {
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

PF_TEST_CASE("isAligned", "[core][pointer]") {
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

PF_TEST_CASE("basic construction and access", "[core][pointers][NonNull]") {
  SECTION("Construct from raw pointer") {
    int value = 42;
    NonNull<int*> nn(&value);
    REQUIRE(*nn == 42);
  }

  SECTION("Dereference operator") {
    int value = 100;
    NonNull<int*> nn(&value);
    REQUIRE(*nn == 100);
    *nn = 200;
    REQUIRE(value == 200);
  }

  SECTION("Member access operator") {
    TestStruct obj;
    NonNull<TestStruct*> nn(&obj);
    REQUIRE(nn->getValue() == 42);
    nn->setValue(100);
    REQUIRE(obj.getValue() == 100);
  }

  SECTION("Implicit conversion to pointer") {
    int value = 50;
    NonNull<int*> nn(&value);
    int* raw = nn; // implicit
    REQUIRE(raw == &value);
  }
}

PF_TEST_CASE("explicit from() factory", "[core][utils][NonNull]") {
  int value = 42;
  NonNull<int*> nn = NonNull<int*>::from(&value);
  REQUIRE(*nn == 42);
  REQUIRE(static_cast<int*>(nn) == &value);
}

PF_TEST_CASE("NonNull copy and move semantics", "[core][utils][NonNull]") {
  SECTION("Copy construction") {
    int value = 42;
    NonNull<int*> nn1(&value);
    NonNull<int*> nn2(nn1);
    REQUIRE(static_cast<int*>(nn2) == static_cast<int*>(nn1));
  }

  SECTION("Move construction") {
    int value = 42;
    NonNull<int*> nn1(&value);
    NonNull<int*> nn2(std::move(nn1));
    REQUIRE(static_cast<int*>(nn2) == &value);
  }

  SECTION("Copy assignment") {
    int a = 1, b = 2;
    NonNull<int*> nn1(&a);
    NonNull<int*> nn2(&b);
    nn2 = nn1;
    REQUIRE(static_cast<int*>(nn2) == &a);
  }

  SECTION("Move assignment") {
    int a = 1, b = 2;
    NonNull<int*> nn1(&a);
    NonNull<int*> nn2(&b);
    nn2 = std::move(nn1);
    REQUIRE(static_cast<int*>(nn2) == &a);
  }
}

PF_TEST_CASE("NonNull in containers", "[core][utils][NonNull]") {
  SECTION("vector of NonNull") {
    std::vector<NonNull<int*>> vec;
    int a = 1, b = 2, c = 3;
    vec.push_back(NonNull<int*>(&a));
    vec.push_back(NonNull<int*>(&b));
    vec.push_back(NonNull<int*>(&c));

    REQUIRE(vec.size() == 3);
    REQUIRE(*vec[0] == 1);
    REQUIRE(*vec[1] == 2);
    REQUIRE(*vec[2] == 3);
  }
}

PF_TEST_CASE("NonNull size and layout", "[core][utils][NonNull]") {
  REQUIRE(sizeof(NonNull<int*>) == sizeof(int*));
  REQUIRE(alignof(NonNull<int*>) == alignof(int*));
}

PF_TEST_CASE("NonNull const correctness", "[core][utils][NonNull]") {
  SECTION("NonNull to const T*") {
    int value = 42;
    NonNull<const int*> nn(&value);
    REQUIRE(*nn == 42);
  }

  SECTION("Const NonNull") {
    int value = 42;
    const NonNull<int*> nn(&value);
    REQUIRE(*nn == 42);
    REQUIRE(static_cast<int*>(nn) == &value);
  }
}

} // namespace pf
