module;

#include <format>
#include <span>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:utils.fmt;
import :require;

namespace pf {

/**
 *@brief Fixed buffer plus the number of characters a formatting call
 * produced (or wanted to produce, when truncated)
 *
 *@tparam T_bufLen storage size in bytes
 */
export template <std::size_t T_bufLen>
struct FmtResult {
  char str[T_bufLen];
  std::size_t size;

  /// Storage size in bytes
  static constexpr std::size_t buffer_size = T_bufLen;
};

/**
 *@brief Formats into a caller provided buffer following std::format
 * semantics
 *
 * The output is truncated to fit @p buf; the return value still reflects
 * the untruncated length. The buffer is NOT null terminated.
 *
 *@tparam V_args argument types deduced against @p format_str
 *
 *@param buf destination span
 *@param format_str format string, compile time checked
 *@param args values to format
 *
 *@return number of characters the full output would occupy, which may
 * exceed buf.size()
 */
export template <class... V_args>
[[nodiscard]] std::size_t
fmt(std::span<char> buf, std::format_string<V_args...> format_str, V_args&&... args) {
  PF_REQUIRE_ASSUME(format_str.get().size() <= buf.size());
  [[maybe_unused]] auto [out, size] =
      std::format_to_n(buf.data(),
                       static_cast<std::iter_difference_t<char*>>(buf.size()),
                       format_str,
                       std::forward<V_args>(args)...);
  return static_cast<std::size_t>(size);
}

/**
 *@brief Convenience overload of fmt() that formats into its own
 * FmtResult<T_bufLen> storage instead of a caller supplied buffer
 *
 *@tparam T_bufLen storage size in bytes
 */
export template <std::size_t T_bufLen, class... V_args>
[[nodiscard]] FmtResult<T_bufLen>
fmt(std::format_string<V_args...> format_str, V_args&&... args) {
  FmtResult<T_bufLen> result;
  result.size = fmt({result.str, T_bufLen}, format_str, std::forward<V_args>(args)...);
  return result;
}

/**
 *@brief Like fmt() but guarantees null termination
 *
 * At most buf.size() - 1 characters are written and a terminator is placed
 * at buf[size], so the string always fits @p buf.
 *
 *@tparam V_args argument types deduced against @p format_str
 *
 *@param buf destination span, must have room for at least one byte beyond
 * the formatted output
 *@param format_str format string, compile time checked
 *@param args values to format
 *
 *@return number of characters written excluding the null terminator
 */
export template <class... V_args>
[[nodiscard]] std::size_t
fmt_cstr(std::span<char> buf,
         std::format_string<V_args...> format_str,
         V_args&&... args) {
  PF_REQUIRE_ASSUME(format_str.get().size() < buf.size());
  [[maybe_unused]] auto [out, size] =
      std::format_to_n(buf.data(),
                       static_cast<std::iter_difference_t<char*>>(buf.size() - 1),
                       format_str,
                       std::forward<V_args>(args)...);
  buf.data()[size] = '\0';
  return static_cast<std::size_t>(size);
}

/**
 *@brief Convenience overload of fmt_cstr() that formats into its own
 * FmtResult<T_bufLen> storage instead of a caller supplied buffer
 *
 *@tparam T_bufLen storage size in bytes
 */
export template <std::size_t T_bufLen, class... V_args>
[[nodiscard]] FmtResult<T_bufLen>
fmt_cstr(std::format_string<V_args...> format_str, V_args&&... args) {
  FmtResult<T_bufLen> result;
  result.size =
      fmt_cstr({result.str, T_bufLen}, format_str, std::forward<V_args>(args)...);
  return result;
}

}
