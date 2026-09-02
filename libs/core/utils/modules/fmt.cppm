module;

#include <format>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:utils.fmt;
import :require;
import :errors;

namespace pf {

namespace detail {

template <typename T_ErrPolicy, typename T_ValueReturnType, typename T_ImplFunc_t>
  requires ErrPolicy_c<T_ErrPolicy, T_ValueReturnType> && std::invocable<T_ImplFunc_t> &&
           std::same_as<T_ValueReturnType, std::invoke_result_t<T_ImplFunc_t>> &&
           requires(const char* str) {
             {
               T_ErrPolicy::fail(str)
             } -> std::same_as<typename T_ErrPolicy::return_type>;
           }
PF_PURE_FUNC [[nodiscard]] T_ErrPolicy::return_type
fmt_structure_impl(T_ImplFunc_t fmt_impl)
    PF_NOEXCEPT_COND(T_ErrPolicy::is_noexcept && !T_ErrPolicy::enabled) {

  if constexpr (!T_ErrPolicy::enabled) {
    return T_ErrPolicy::success(fmt_impl());
  }

  try {
    return T_ErrPolicy::success(fmt_impl());
  } catch (std::exception& e) {
    return T_ErrPolicy::fail(e.what());
  } catch (Exception& e) {
    return T_ErrPolicy::fail(e.what());
  } catch (...) {
    return T_ErrPolicy::fail("fmt failed with unkown exception");
  }
  std::unreachable();
}

}

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

export struct FmtError : Exception {
  FmtError(const char* what) : Exception(what) {}
};

/**
 * @brief Formats into a caller provided buffer following std::format
 * semantics
 *
 * The output is truncated to fit @p buf; the return value still reflects
 * the untruncated length. The buffer is NOT null terminated.
 *
 * @error_handling Fails upon an exception being thrown by the formatter.
 * if this inherits from std::exception or pf::exception it passes the
 * value of what to the fail.
 *
 * @tparam T_ErrPolicy the error policy for this function
 * @tparam V_args argument types deduced against @p format_str
 *
 * @param buf destination span
 * @param format_str format string, compile time checked
 * @param args values to format
 *
 * @return number of characters the full output would occupy, which may
 * exceed buf.size()
 */
export template <typename T_ErrPolicy = ErrPolicy_throws<std::size_t, FmtError>,
                 class... V_args>
  requires ErrPolicy_c<T_ErrPolicy, std::size_t> && requires(const char* str) {
    { T_ErrPolicy::fail(str) } -> std::same_as<typename T_ErrPolicy::return_type>;
  }
PF_PURE_FUNC [[nodiscard]] T_ErrPolicy::return_type
fmt(std::span<char> buf, std::format_string<V_args...> format_str, V_args&&... args)
    PF_NOEXCEPT_COND(T_ErrPolicy::is_noexcept && !T_ErrPolicy::enabled) {
  PF_REQUIRE_ASSUME(format_str.get().size() <= buf.size());

  auto fmt_impl = [&]() {
    [[maybe_unused]] auto [out, size] =
        std::format_to_n(buf.data(),
                         static_cast<std::iter_difference_t<char*>>(buf.size()),
                         format_str,
                         std::forward<V_args>(args)...);
    return T_ErrPolicy::success(static_cast<std::size_t>(size));
  };

  return detail::fmt_structure_impl<T_ErrPolicy, std::size_t>(fmt_impl);
}

/**
 * @brief Unchecked version of fmt()
 */
export template <class... V_args>
PF_PURE_FUNC [[nodiscard]] std::size_t
fmt_unchecked(std::span<char> buf,
              std::format_string<V_args...> format_str,
              V_args&&... args) {
  static constexpr std::string_view fail_msg = "format error";
  return fmt<ErrPolicy_nothing<std::size_t, fail_msg>>(
      buf, format_str, std::forward<V_args>(args)...);
}

/**
 * @brief Try version of fmt() that returns std::optional<std::size_t>
 *
 *@error_handling Catches all exceptions and returns std::nullopt instead
 */
export template <class... V_args>
PF_PURE_FUNC [[nodiscard]] std::optional<std::size_t>
try_fmt(std::span<char> buf,
        std::format_string<V_args...> format_str,
        V_args&&... args) PF_NOEXCEPT {
  return fmt<ErrPolicy_optional<std::size_t>>(
      buf, format_str, std::forward<V_args>(args)...);
}

/** *@brief Convenience overload of fmt() that formats into its own
 * FmtResult<T_bufLen> storage instead of a caller supplied buffer
 *
 *@tparam T_bufLen storage size in bytes
 */
export template <std::size_t T_bufLen,
                 typename T_ErrPolicy = ErrPolicy_throws<FmtResult<T_bufLen>, FmtError>,
                 class... V_args>
  requires ErrPolicy_c<T_ErrPolicy, FmtResult<T_bufLen>> && requires(const char* str) {
    { T_ErrPolicy::fail(str) } -> std::same_as<typename T_ErrPolicy::return_type>;
  }
PF_PURE_FUNC [[nodiscard]] T_ErrPolicy::return_type
fmt(std::format_string<V_args...> format_str, V_args&&... args)
    PF_NOEXCEPT_COND(T_ErrPolicy::is_noexcept && !T_ErrPolicy::enabled) {
  FmtResult<T_bufLen> result;

  auto fmt_impl = [&]() {
    result.size = fmt_unchecked(
        {result.str, result.buffer_size}, format_str, std::forward<V_args>(args)...);
    return result;
  };

  return detail::fmt_structure_impl<T_ErrPolicy, FmtResult<T_bufLen>>(fmt_impl);
}

export template <std::size_t T_bufLen, class... V_args>
PF_PURE_FUNC [[nodiscard]] FmtResult<T_bufLen>
fmt_unchecked(std::format_string<V_args...> format_str, V_args&&... args) {
  static constexpr std::string_view fail_msg = "format error";
  return fmt<T_bufLen, ErrPolicy_nothing<FmtResult<T_bufLen>, fail_msg>>(
      format_str, std::forward<V_args>(args)...);
}

export template <std::size_t T_bufLen, class... V_args>
PF_PURE_FUNC [[nodiscard]] std::optional<FmtResult<T_bufLen>>
try_fmt(std::format_string<V_args...> format_str, V_args&&... args) PF_NOEXCEPT {
  return fmt<T_bufLen, ErrPolicy_optional<FmtResult<T_bufLen>>>(
      format_str, std::forward<V_args>(args)...);
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
export template <typename T_ErrPolicy = ErrPolicy_throws<std::size_t, FmtError>,
                 class... V_args>
  requires ErrPolicy_c<T_ErrPolicy, std::size_t> && requires(const char* str) {
    { T_ErrPolicy::fail(str) } -> std::same_as<typename T_ErrPolicy::return_type>;
  }
PF_PURE_FUNC [[nodiscard]] T_ErrPolicy::return_type
fmt_cstr(std::span<char> buf, std::format_string<V_args...> format_str, V_args&&... args)
    PF_NOEXCEPT_COND(T_ErrPolicy::is_noexcept && !T_ErrPolicy::enabled) {
  PF_REQUIRE_ASSUME(format_str.get().size() < buf.size());

  auto fmt_impl = [&]() {
    [[maybe_unused]] auto [out, size] =
        std::format_to_n(buf.data(),
                         static_cast<std::iter_difference_t<char*>>(buf.size() - 1),
                         format_str,
                         std::forward<V_args>(args)...);
    buf.data()[size] = '\0';
    return static_cast<std::size_t>(size);
  };

  return detail::fmt_structure_impl<T_ErrPolicy, std::size_t>(fmt_impl);
}

export template <class... V_args>
PF_PURE_FUNC [[nodiscard]] std::size_t
fmt_cstr_unchecked(std::span<char> buf,
                   std::format_string<V_args...> format_str,
                   V_args&&... args) {
  PF_REQUIRE_ASSUME(format_str.get().size() < buf.size());
  static constexpr std::string_view msg = "fmt error";
  return fmt_cstr<ErrPolicy_nothing<std::size_t, msg>>(
      buf, format_str, std::forward<V_args>(args)...);
}

export template <class... V_args>
PF_PURE_FUNC [[nodiscard]] std::optional<std::size_t>
try_fmt_cstr(std::span<char> buf,
             std::format_string<V_args...> format_str,
             V_args&&... args)
    PF_NOEXCEPT_COND(T_ErrPolicy::is_noexcept && !T_ErrPolicy::enabled) {
  PF_REQUIRE_ASSUME(format_str.get().size() < buf.size());
  return fmt_cstr<ErrPolicy_optional<std::size_t>>(
      buf, format_str, std::forward<V_args>(args)...);
}

export template <std::size_t T_bufLen,
                 typename T_ErrPolicy = ErrPolicy_throws<FmtResult<T_bufLen>, FmtError>,
                 class... V_args>
  requires ErrPolicy_c<T_ErrPolicy, FmtResult<T_bufLen>> && requires(const char* str) {
    { T_ErrPolicy::fail(str) } -> std::same_as<typename T_ErrPolicy::return_type>;
  }
PF_PURE_FUNC [[nodiscard]] T_ErrPolicy::return_type
fmt_cstr(std::format_string<V_args...> format_str, V_args&&... args)
    PF_NOEXCEPT_COND(T_ErrPolicy::is_noexcept && !T_ErrPolicy::enabled) {
  FmtResult<T_bufLen> result;

  auto fmt_impl = [&]() {
    result.size = fmt_cstr_unchecked(
        {result.str, result.buffer_size}, format_str, std::forward<V_args>(args)...);
    return result;
  };

  return detail::fmt_structure_impl<T_ErrPolicy, FmtResult<T_bufLen>>(fmt_impl);
}

export template <std::size_t T_bufLen, class... V_args>
PF_PURE_FUNC [[nodiscard]] FmtResult<T_bufLen>
fmt_cstr_unchecked(std::format_string<V_args...> format_str, V_args&&... args) {
  static constexpr std::string_view fail_msg = "format error";
  return fmt<T_bufLen, ErrPolicy_nothing<FmtResult<T_bufLen>, fail_msg>>(
      format_str, std::forward<V_args>(args)...);
}

export template <std::size_t T_bufLen, class... V_args>
PF_PURE_FUNC [[nodiscard]] std::optional<FmtResult<T_bufLen>>
try_cstr_fmt(std::format_string<V_args...> format_str, V_args&&... args) PF_NOEXCEPT {
  return fmt<T_bufLen, ErrPolicy_optional<FmtResult<T_bufLen>>>(
      format_str, std::forward<V_args>(args)...);
}

}
