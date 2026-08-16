module;

#include <format>
#include <span>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:utils.fmt;
import :require;

namespace pf {

export template <std::size_t T_bufLen>
struct FmtResult {
  char str[T_bufLen];
  std::size_t size;

  static constexpr std::size_t buffer_size = T_bufLen;
};

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

export template <std::size_t T_bufLen, class... V_args>
[[nodiscard]] FmtResult<T_bufLen>
fmt(std::format_string<V_args...> format_str, V_args&&... args) {
  FmtResult<T_bufLen> result;
  result.size = fmt({result.str, T_bufLen}, format_str, std::forward<V_args>(args)...);
  return result;
}

export template <class... V_args>
[[nodiscard]] std::size_t
fmt_cstr(std::span<char> buf, std::format_string<V_args...> format_str, V_args&&... args) {
  PF_REQUIRE_ASSUME(format_str.get().size() < buf.size());
  [[maybe_unused]] auto [out, size] =
    std::format_to_n(buf.data(),
                     static_cast<std::iter_difference_t<char*>>(buf.size() - 1),
                     format_str,
                     std::forward<V_args>(args)...);
  buf.data()[size] = '\0';
  return static_cast<std::size_t>(size);
}

export template <std::size_t T_bufLen, class... V_args>
[[nodiscard]] FmtResult<T_bufLen>
fmt_cstr(std::format_string<V_args...> format_str, V_args&&... args) {
  FmtResult<T_bufLen> result;
  result.size = fmt_cstr({result.str, T_bufLen}, format_str, std::forward<V_args>(args)...);
  return result;
}

}
