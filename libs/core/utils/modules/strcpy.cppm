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
export template <typename T, typename ValT>
concept IsTerminator_c = requires(T is_terminator_func, ValT val) {
  { is_terminator_func(val) } -> std::convertible_to<bool>;
};

/**
 * @brief Copies by value each element, from src to dest until:
 *        1. end of @p src 2. end of @p dest 3. the amount of elements copied
 *        that satisfy @p isTerminator is greater than @p maxTerminators
 *
 *@tparam T element type
 *@tparam IsTerminatorFunc type of the terminator predicate
 *
 *@param dest destination buffer, its size bounds the copy
 *@param src source buffer
 *@param isTerminator predicate signalling terminator elements
 *@param maxTerminators copy stops once this many terminators have been
 * seen; 0 disables the limit
 *
 * @return The number of elements copied, including the terminators
 */
export template <typename T, IsTerminator_c<T> IsTerminatorFunc>
constexpr std::size_t
copy_until(std::span<T> dest,
           const std::span<const T> src,
           IsTerminatorFunc&& is_terminator,
           const std::size_t max_terminators = 0) PF_NOEXCEPT {
  std::size_t const max_count = std::min(dest.size(), src.size());
  std::size_t i = 0;
  std::size_t terminator_count = 0;
  for (i = 0; i < max_count; i++) {
    if (is_terminator(src[i])) terminator_count++;

    if (max_terminators < terminator_count) break;

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
       const std::size_t max_null_terminators = 0) PF_NOEXCEPT {
  const std::size_t copy_count = copy_until(
      {dest.data(), dest.size() - 1},
      std::span<const char>{src.data(), src.size()},
      [](char val) { return val == '\0'; },
      max_null_terminators);
  dest[copy_count] = '\0';
  return copy_count + 1;
}

}
