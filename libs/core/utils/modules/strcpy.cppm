module;

#include <span>
#include <string_view>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:utils.strcpy;

import :require;

namespace pf {

/**
 *@brief Concept for callables deciding whether an element is a terminator
 *
 *@tparam T element type examined
 *@tparam T_val the value passed to the predicate
 */
export template <typename T, typename T_val>
concept IsTerminator_c = requires(T isTerminatorFunc, T_val val) {
  { isTerminatorFunc(val) } -> std::convertible_to<bool>;
};

/**
 * @brief Copies by value each element, from src to dest until:
 *        1. end of @p src 2. end of @p dest 3. the amount of elements copied
 *        that satisfy @p isTerminator is greater than @p maxTerminators
 *
 *@tparam T element type
 *@tparam T_isTerminatorFunc type of the terminator predicate
 *
 *@param dest destination buffer, its size bounds the copy
 *@param src source buffer
 *@param isTerminator predicate signalling terminator elements
 *@param maxTerminators copy stops once this many terminators have been
 * seen; 0 disables the limit
 *
 * @return The number of elements copied, including the terminators
 */
export template <typename T, IsTerminator_c<T> T_isTerminatorFunc>
constexpr std::size_t
copy_until(std::span<T> dest,
           const std::span<const T> src,
           T_isTerminatorFunc&& isTerminator,
           const std::size_t maxTerminators = 0) PF_NOEXCEPT {
  std::size_t const maxCount = std::min(dest.size(), src.size());
  std::size_t i;
  std::size_t terminatorCount = 0;
  for (i = 0; i < maxCount; i++) {
    if (isTerminator(src[i])) terminatorCount++;

    if (maxTerminators < terminatorCount) break;

    dest[i] = src[i];
  }

  return i;
}

/**
 * @brief A safe copy function for strings that respects buffer sizes and
 *        null terminators. Copies until the end of the end of @p dest buffer or
 * the end of @p src buffer or a null terminator in @p src, while guaranteeing
 * that @p dest is null terminated
 *
 *@param dest destination buffer, always null terminated on return
 *@param src source string
 *@param maxNullTerminators stop early after this many terminators in
 * @p src; 0 disables the limit
 *
 * @return The number of characters copied, including the null terminator
 */
export constexpr std::size_t
strcpy(std::span<char> dest,
       const std::string_view src,
       const std::size_t maxNullTerminators = 0) PF_NOEXCEPT {
  const std::size_t copyCount = copy_until(
      {dest.data(), dest.size() - 1},
      std::span<const char>{src.data(), src.size()},
      [](char val) { return val == '\0'; },
      maxNullTerminators);
  dest[copyCount] = '\0';
  return copyCount + 1;
}

}
