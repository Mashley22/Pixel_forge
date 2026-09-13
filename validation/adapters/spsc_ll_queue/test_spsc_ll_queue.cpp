#include <cstdlib>
#include <queue>

#include <catch2/catch_test_macros.hpp>

#include <PixelForgeValidationHelpers/helpers.hpp>

import PixelForge.adapters;
import PixelForge.core;
import PixelForge.validation_helpers;

namespace pf::adapters {

namespace {

template<typename T>
class NodeStorage {
  public:
    using Node = SPSCLLQueue<T>::storage_type;

  NodeStorage() {
    buffer.size = sizeof(Node);
    buffer.data = pointer_cast<std::byte*>(std::aligned_alloc(sizeof(Node), alignof(Node)));
  }

  ObjectStorage<Node> objStore() { return buffer.asObjects<Node>(1); }

  ~NodeStorage() {
    std::free(buffer.data);
  }

  Buffer buffer;
  
};

}

PF_TEST_CASE("Basic operation" , "[adapters][SPSCLLQueue]") {
  std::queue<NodeStorage<std::size_t>> storageQueue;

  auto addStorage = [&]() {
    storageQueue.emplace();
    return storageQueue.back().objStore();
  };

  SECTION("some pushes and pops") {
    SPSCLLQueue<std::size_t> queue(addStorage());
    REQUIRE(queue.empty());
    constexpr std::size_t count = 5;

    for (std::size_t i = 0; i < count; i++) {
      queue.emplace(addStorage(), i);
      REQUIRE(!queue.empty());
    }

    for (std::size_t i = 0; i < count; i++) {
      REQUIRE(!queue.empty());
      auto node = queue.pop();
      REQUIRE(node->val == i);
      
      REQUIRE(pointer_cast<std::byte*>(node.get()) == storageQueue.front().buffer.data);
      storageQueue.pop();
    }
  }
}

}
