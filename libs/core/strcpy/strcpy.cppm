module;

#include <string_view>
#include <span>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:strcpy;

import PixelForge.core.require;

namespace pf {

template<typename T, typename T_val>
concept IsTerminator_c = requires(T isTerminatorFunc, T_val val) {
  { isTerminatorFunc(val) } -> std::convertible_to<bool>;
};

/**
 * @brief Copies by value each element, from src to dest until:
 *        1. end of @p src 2. end of @p dest 3. the amount of elements copied
 *        that satisfy @p isTerminator is greater than @p maxTerminators
 *
 * @return The number of elements copied, including the terminators
 */
template<typename T, IsTerminator_c<T> T_isTerminatorFunc>
[[nodiscard]] 
constexpr std::size_t 
copy_until(std::span<T> dest, const std::span<const T> src, T_isTerminatorFunc&& isTerminator, const std::size_t maxTerminators = 0) PF_NOEXCEPT {
  std::size_t maxCount = std::min(dest.size(), src.size());
  std::size_t i;
  std::size_t terminatorCount = 0;
  for (i = 0; i < maxCount; i++) {
    if (isTerminator(src[i]))
      terminatorCount++;

    if (maxTerminators < terminatorCount) 
      break;

    dest[i] = src[i];
  }

  return i + 1;
}

/**
 * @brief A safe copy function for strings that respects buffer sizes and
 *        null terminators. Copies until the end of the end of @p dest buffer or the end of @p src buffer or
 *        a null terminator in @p src, while guaranteeing
 *        that @p dest is null terminated
 *
 * @return The number of characters copied, including the null terminator
 */
[[nodiscard]] 
constexpr std::size_t 
strcpy(std::span<char> dest, const std::span<const char> src, const std::size_t maxNullTerminators = 0) PF_NOEXCEPT {
  std::size_t copyCount = copy_until({dest.data(), dest.size() - 1}, src, [](char val){ return val == '\0'; }, maxNullTerminators);
  dest.back() = '\0';
  return copyCount;
}

[[nodiscard]] 
constexpr std::size_t 
strcpy(std::span<char> dest, const std::string_view src, const std::size_t maxNullTerminators = 0) PF_NOEXCEPT {
  return strcpy(dest, std::span<const char>{src.data(), src.size()}, maxNullTerminators);
}

}
