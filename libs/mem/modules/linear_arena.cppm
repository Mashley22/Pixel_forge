module;

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include <PixelForge/core/macros.hpp>

export module PixelForge.mem:linearArena;

import :exception;
import :align;
import PixelForge.core;
import PixelForge.adapters;

namespace pf {

namespace mem {

export class LinearArena {
public:
  using size_type = std::size_t;
  using storage_type = std::byte;

  class OOMError : ::pf::mem::OOMError {
  public:
    OOMError() = delete;
    OOMError(std::size_t requested, std::size_t needed, std::size_t available) PF_NOEXCEPT
      : ::pf::mem::OOMError("Linear Arena", requested, needed, available) {}
  };

  LinearArena() = delete;
  constexpr LinearArena(const ObjectStorage<storage_type> storage) PF_NOEXCEPT
    : m_stack(storage) {}

  [[nodiscard]] constexpr size_type
  capacity() const PF_NOEXCEPT {
    return m_stack.capacity();
  }

  [[nodiscard]] constexpr size_type
  in_use() const PF_NOEXCEPT {
    return m_stack.size();
  }

  [[nodiscard]] constexpr size_type
  remaining() const PF_NOEXCEPT {
    return m_stack.remaining();
  }

  template <typename T_ErrPolicy = ErrPolicy_throws<storage_type*, OOMError>>
    requires ErrPolicy_c<T_ErrPolicy, storage_type*> &&
             requires(std::size_t requested, std::size_t needed, std::size_t remaining) {
               {
                 T_ErrPolicy::fail(requested, needed, remaining)
               } -> std::same_as<typename T_ErrPolicy::return_type>;
             }
  [[nodiscard]] constexpr T_ErrPolicy::return_type
  alloc(size_type amount) PF_NOEXCEPT_COND(T_ErrPolicy::is_noexcept) {
    PF_CHECK_ERR_POLICY(T_ErrPolicy, remaining() < amount, amount, amount, remaining());
    const auto retVal = m_stack.data() + m_stack.size();
    for (std::size_t i = 0; i < amount; i++) {
      m_stack.emplace_unchecked();
    }
    return T_ErrPolicy::success(retVal);
  }

  [[nodiscard]] constexpr storage_type*
  alloc_unchecked(size_type amount) PF_NOEXCEPT {
    static constexpr std::string_view what_arg = "Linear arena oom error";
    return alloc<ErrPolicy_nothing<storage_type*, what_arg>>(amount);
  }

  [[nodiscard]] constexpr std::optional<storage_type*>
  try_alloc(size_type amount) PF_NOEXCEPT {
    return alloc<ErrPolicy_optional<storage_type*>>(amount);
  }

  template <typename T_ErrPolicy = ErrPolicy_throws<storage_type*, OOMError>>
    requires ErrPolicy_c<T_ErrPolicy, storage_type*> &&
             requires(std::size_t requested, std::size_t needed, std::size_t remaining) {
               {
                 T_ErrPolicy::fail(requested, needed, remaining)
               } -> std::same_as<typename T_ErrPolicy::return_type>;
             }
  [[nodiscard]] constexpr T_ErrPolicy::return_type
  alloc(size_type amount, size_type alignment)
      PF_NOEXCEPT_COND(T_ErrPolicy::is_noexcept) {
    PF_REQUIRE_ASSUME(std::has_single_bit(alignment));
    const std::size_t padding = alignmentPadding(
        pointer_cast<std::uintptr_t>(m_stack.data() + m_stack.size()), alignment);

    const std::size_t required = amount + padding;
    PF_CHECK_ERR_POLICY(T_ErrPolicy, remaining() < amount, amount, required, remaining());
    [[maybe_unused]] const auto _ = alloc_unchecked(padding);
    return T_ErrPolicy::success(alloc_unchecked(amount));
  }

  [[nodiscard]] constexpr storage_type*
  alloc_unchecked(size_type amount, size_type alignment) PF_NOEXCEPT {
    static constexpr std::string_view what_arg = "Linear arena oom error";
    return alloc<ErrPolicy_nothing<storage_type*, what_arg>>(amount, alignment);
  }

  [[nodiscard]] constexpr std::optional<storage_type*>
  try_alloc(size_type amount, std::size_t alignment) PF_NOEXCEPT {
    return alloc<ErrPolicy_optional<storage_type*>>(amount, alignment);
  }

private:
  adapters::Stack<std::byte> m_stack;
};

}

}
