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
import PixelForge.containers;

namespace pf {

namespace mem {

export class LinearArena {
public:
  using SizeType = std::size_t;
  using StorageType = std::byte;

  class OOMError : ::pf::mem::OOMError {
  public:
    OOMError() = delete;
    OOMError(std::size_t requested, std::size_t needed, std::size_t available) PF_NOEXCEPT
      : ::pf::mem::OOMError("Linear Arena", requested, needed, available) {}
  };

  LinearArena() = delete;
  constexpr LinearArena(const ObjectStorage<StorageType> storage) PF_NOEXCEPT
    : m_stack(storage) {}

  [[nodiscard]] constexpr SizeType
  capacity() const PF_NOEXCEPT {
    return m_stack.capacity();
  }

  [[nodiscard]] constexpr SizeType
  in_use() const PF_NOEXCEPT {
    return m_stack.size();
  }

  [[nodiscard]] constexpr SizeType
  remaining() const PF_NOEXCEPT {
    return m_stack.remaining();
  }

  template <typename ErrPolicy = ErrPolicyThrows<StorageType*, OOMError>>
    requires ErrPolicy_c<ErrPolicy, StorageType*> &&
             requires(std::size_t requested, std::size_t needed, std::size_t remaining) {
               {
                 ErrPolicy::fail(requested, needed, remaining)
               } -> std::same_as<typename ErrPolicy::ReturnType>;
             }
  [[nodiscard]] constexpr ErrPolicy::ReturnType
  alloc(SizeType amount) PF_NOEXCEPT_COND(ErrPolicy::is_noexcept) {
    PF_CHECK_ERR_POLICY(ErrPolicy, remaining() < amount, amount, amount, remaining());
    const auto ret_val = m_stack.data() + m_stack.size();
    for (std::size_t i = 0; i < amount; i++) {
      m_stack.emplace_unchecked();
    }
    return ErrPolicy::success(ret_val);
  }

  [[nodiscard]] constexpr StorageType*
  alloc_unchecked(SizeType amount) PF_NOEXCEPT {
    static constexpr std::string_view what_arg = "Linear arena oom error";
    return alloc<ErrPolicyNothing<StorageType*, what_arg>>(amount);
  }

  [[nodiscard]] constexpr std::optional<StorageType*>
  try_alloc(SizeType amount) PF_NOEXCEPT {
    return alloc<ErrPolicyOptional<StorageType*>>(amount);
  }

  template <typename ErrPolicy = ErrPolicyThrows<StorageType*, OOMError>>
    requires ErrPolicy_c<ErrPolicy, StorageType*> &&
             requires(std::size_t requested, std::size_t needed, std::size_t remaining) {
               {
                 ErrPolicy::fail(requested, needed, remaining)
               } -> std::same_as<typename ErrPolicy::ReturnType>;
             }
  [[nodiscard]] constexpr ErrPolicy::ReturnType
  alloc(SizeType amount, SizeType alignment) PF_NOEXCEPT_COND(ErrPolicy::is_noexcept) {
    PF_REQUIRE_ASSUME(std::has_single_bit(alignment));
    const std::size_t padding = alignment_padding(
        pointer_cast<std::uintptr_t>(m_stack.data() + m_stack.size()), alignment);

    const std::size_t required = amount + padding;
    PF_CHECK_ERR_POLICY(ErrPolicy, remaining() < amount, amount, required, remaining());
    [[maybe_unused]] const auto _ = alloc_unchecked(padding);
    return ErrPolicy::success(alloc_unchecked(amount));
  }

  [[nodiscard]] constexpr StorageType*
  alloc_unchecked(SizeType amount, SizeType alignment) PF_NOEXCEPT {
    static constexpr std::string_view what_arg = "Linear arena oom error";
    return alloc<ErrPolicyNothing<StorageType*, what_arg>>(amount, alignment);
  }

  [[nodiscard]] constexpr std::optional<StorageType*>
  try_alloc(SizeType amount, std::size_t alignment) PF_NOEXCEPT {
    return alloc<ErrPolicyOptional<StorageType*>>(amount, alignment);
  }

private:
  adapters::Stack<std::byte> m_stack{};
};

}

}
