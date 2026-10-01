module;

#include <concepts>
#include <memory_resource>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:mem_resource;

import :buffer;

export namespace pf::mem {

template <typename Resource>
concept Resource_c = requires(Resource& resource,
    std::size_t alignment, std::size_t size, const Buffer& buffer) {

  { resource.allocate(size, alignment) } -> std::convertible_to<Buffer>;
  { resource.allocate(size) } -> std::convertible_to<Buffer>;
  { resource.deallocate(buffer) } -> std::convertible_to<void>;
};

template<typename T>
requires std::derived_from<T, std::pmr::memory_resource>
// NOLINTNEXTLINE
class std_memory_resource_adapter {
  public:
    std_memory_resource_adapter() PF_NOEXCEPT = default;
    std_memory_resource_adapter(T* ptr) : m_resource(ptr) {}

    [[nodiscard]] Buffer 
    allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) {
      return Buffer{
        .data = m_resource->allocate(size, alignment),
        .size = size
      };
    }

    void
    deallocate(const Buffer& buffer) {
      m_resource->deallocate(static_cast<void*>(buffer.data));
    }
    
    // dont let it instantiate this for non pmr adapters
    template<typename Other>
    requires std::derived_from<Other, std::pmr::memory_resource>
    [[nodiscard]] bool
    is_interoperable(const std_memory_resource_adapter<Other>& other) {
      return m_resource->is_equal(*other.m_resource);
    }

  private:
    T* m_resource{nullptr};
};

/**
 * @brief No sleeping locks, no syscall
 */
template <Resource_c T>
struct is_fast_resource {
    static constexpr bool value = []() {
      if constexpr (requires {
        { T::is_fast } -> std::convertible_to<bool>;
      }) {
        return T::is_fast;
      }
      return false;
    }();
};

template <typename T>
using is_fast_resource_v = is_fast_resource<T>::value;

template <Resource_c T>
struct is_noexcept_resource {
    static constexpr bool value = []() {
      if constexpr (requires {
        { T::is_noexcept } -> std::convertible_to<bool>;
      }) {
        return T::is_noexcept;
      }
      return false;
    }();
};

template <Resource_c T>
using is_noexcept_resource_v = is_noexcept_resource<T>::value;

/**
 * @brief checks if @ref rhs is compatible with @ref lhs
 * i.e. memory allocated with lhs can be freed with lhs and vice-versa
 *
 * @important This is not neccessarily a symmetric comparison, although
 * in principle it can always be.
 *
 * @param lhs
 * @param rhs
 */
template <Resource_c A, Resource_c B>
[[nodiscard]] bool is_interoperable_resource(const A& lhs, const B& rhs) {
  if constexpr (requires {
      { lhs.is_interoperable(rhs) } -> std::convertible_to<bool>;
  }) {
    return lhs.is_interoperable(rhs);
  }
  return false;
}

}
