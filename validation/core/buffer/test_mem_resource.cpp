#include <memory_resource>

#include <catch2/catch_test_macros.hpp>

#include <PixelForgeValidationHelpers/helpers.hpp>

import PixelForge.core;

namespace pf::mem {

namespace {

struct Dummy {
  Buffer
  allocate(std::size_t, std::size_t = 0) {
    return {};
  }

  void
  deallocate(Buffer) {}
};

}

PF_TEST_CASE("comparison with pmr!", "[core][mem][resource]") {
  STATIC_REQUIRE(Resource_c<std_memory_resource_adapter<std::pmr::memory_resource>>);
  STATIC_REQUIRE(
      Resource_c<std_memory_resource_adapter<std::pmr::monotonic_buffer_resource>>);

  REQUIRE_FALSE(is_interoperable_resource(Dummy{}, Dummy{}));
  REQUIRE_FALSE(is_interoperable_resource(
      Dummy{}, std_memory_resource_adapter<std::pmr::monotonic_buffer_resource>{}));
}

}
