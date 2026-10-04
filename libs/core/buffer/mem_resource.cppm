module;

#include <concepts>
#include <memory_resource>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:mem_resource;

import :buffer;

export namespace pf::mem {

template <typename Resource>
concept Resource_c = requires(
    Resource& resource, std::size_t alignment, std::size_t size, const Buffer& buffer) {
  { resource.allocate(size, alignment) } -> std::convertible_to<Buffer>;
  { resource.allocate(size) } -> std::convertible_to<Buffer>;
  { resource.deallocate(buffer) } -> std::convertible_to<void>;
};

/**
 * @brief a resource that can report that it is null, e.g. one default
 * constructed over a null underlying resource
 */
template <typename Resource>
concept NullableResource_c = requires(Resource& resource) {
  { resource.is_null() } -> std::convertible_to<bool>;
};

/**
 * @brief a reference to a memory resource living elsewhere, it forwards the
 * resource interface to the resource it refers to
 *
 * Useful for handing a resource to something that takes ownership of a
 * resource without actually taking ownership of it, e.g. a container
 * parameterised on ResourceRef<SomeResource>.
 *
 * @important the referenced resource must outlive the reference
 */
template <Resource_c Resource>
class ResourceRef {
public:
  explicit ResourceRef(Resource& resource) PF_NOEXCEPT : m_resource(&resource) {
    if constexpr (NullableResource_c<Resource>) {
      PF_REQUIRE(!resource.is_null(),
                 "ResourceRef: cannot reference a null memory resource");
    }
  }

  ResourceRef(const ResourceRef&) = default;
  ResourceRef(ResourceRef&&) PF_NOEXCEPT = default;
  ResourceRef&
  operator=(const ResourceRef&) = default;
  ResourceRef&
  operator=(ResourceRef&&) PF_NOEXCEPT = default;
  ~ResourceRef() = default;

  [[nodiscard]] Buffer
  allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) {
    return m_resource->allocate(size, alignment);
  }

  void
  deallocate(const Buffer& buffer) {
    m_resource->deallocate(buffer);
  }

  /**
   * @brief whether the referenced resource can be called at all, present only
   * when the referenced resource reports its own nullness
   */
  [[nodiscard]] bool
  is_null() const PF_NOEXCEPT
    requires NullableResource_c<Resource>
  {
    return m_resource->is_null();
  }

  template <Resource_c Other>
    requires requires(const Resource& resource, const Other& other_resource) {
      { resource.is_interoperable(other_resource) } -> std::convertible_to<bool>;
    }
  [[nodiscard]] bool
  is_interoperable(const ResourceRef<Other>& other) const {
    return m_resource->is_interoperable(other.get());
  }

  /** mirrors the noexcept-ness of the referenced resource */
  static constexpr bool is_noexcept = []() {
    if constexpr (requires {
                    { Resource::is_noexcept } -> std::convertible_to<bool>;
                  }) {
      return Resource::is_noexcept;
    }
    return false;
  }();

  /** the referenced resource itself */
  [[nodiscard]] Resource&
  get() PF_NOEXCEPT {
    return *m_resource;
  }

  [[nodiscard]] const Resource&
  get() const PF_NOEXCEPT {
    return *m_resource;
  }

private:
  /** not owned, the referenced resource must outlive this reference */
  [[no_unique_address]] Resource* m_resource;
};

template <typename T>
  requires std::derived_from<T, std::pmr::memory_resource>
// NOLINTNEXTLINE
class std_memory_resource_adapter {
public:
  std_memory_resource_adapter() PF_NOEXCEPT = default;
  std_memory_resource_adapter(T* ptr) : m_resource(ptr) {}

  [[nodiscard]] Buffer
  allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) {
    return Buffer{.data = m_resource->allocate(size, alignment), .size = size};
  }

  void
  deallocate(const Buffer& buffer) {
    m_resource->deallocate(static_cast<void*>(buffer.data));
  }

  // dont let it instantiate this for non pmr adapters
  template <typename Other>
    requires std::derived_from<Other, std::pmr::memory_resource>
  [[nodiscard]] bool
  is_interoperable(const std_memory_resource_adapter<Other>& other) const {
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
[[nodiscard]] bool
is_interoperable_resource(const A& lhs, const B& rhs) {
  if constexpr (requires {
                  { lhs.is_interoperable(rhs) } -> std::convertible_to<bool>;
                }) {
    return lhs.is_interoperable(rhs);
  }
  return false;
}

}
