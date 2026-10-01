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

template <typename ErrPolicy, typename ValueReturnType, typename ImplFunc>
  requires ErrPolicy_c<ErrPolicy, ValueReturnType> && std::invocable<ImplFunc> &&
           std::same_as<ValueReturnType, std::invoke_result_t<ImplFunc>> &&
           requires(const char* str) {
             { ErrPolicy::fail(str) } -> std::same_as<typename ErrPolicy::ReturnType>;
           }
PF_PURE_FUNC [[nodiscard]] ErrPolicy::ReturnType
fmt_structure_impl(ImplFunc fmt_impl)
    PF_NOEXCEPT_COND(ErrPolicy::is_noexcept && !ErrPolicy::enabled) {

  if constexpr (!ErrPolicy::enabled) {
    return ErrPolicy::success(fmt_impl());
  }

  try {
    return ErrPolicy::success(fmt_impl());
  } catch (std::exception& e) {
    return ErrPolicy::fail(e.what());
  } catch (Exception& e) {
    return ErrPolicy::fail(e.what());
  } catch (...) {
    return ErrPolicy::fail("fmt failed with unkown exception");
  }
  std::unreachable();
}

}

/**
 *@brief Fixed buffer plus the number of characters a formatting call
 * produced (or wanted to produce, when truncated)
 *
 *@tparam BufLen storage size in bytes
 */
export template <std::size_t BufLen>
struct FmtResult {
  std::array<char, BufLen> str;
  std::size_t size;

  /// Storage size in bytes
  static constexpr std::size_t buffer_size = BufLen;

  [[nodiscard]] constexpr std::string_view
  to_str_view() const PF_NOEXCEPT {
    return std::string_view(str.data(), size);
  }
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
 * @tparam ErrPolicy the error policy for this function
 * @tparam V_args argument types deduced against @p format_str
 *
 * @param buf destination span
 * @param format_str format string, compile time checked
 * @param args values to format
 *
 * @return number of characters the full output would occupy, which may
 * exceed buf.size()
 */
export template <typename ErrPolicy = ErrPolicyThrows<std::size_t, FmtError>,
                 class... VArgs>
  requires ErrPolicy_c<ErrPolicy, std::size_t> && requires(const char* str) {
    { ErrPolicy::fail(str) } -> std::same_as<typename ErrPolicy::ReturnType>;
  }
PF_PURE_FUNC [[nodiscard]] ErrPolicy::ReturnType
fmt(std::span<char> buf, std::format_string<VArgs...> format_str, VArgs&&... args)
    PF_NOEXCEPT_COND(ErrPolicy::is_noexcept && !ErrPolicy::enabled) {
  PF_REQUIRE_ASSUME(format_str.get().size() <= buf.size());

  auto fmt_impl = [&]() {
    [[maybe_unused]] auto [out, size] =
        std::format_to_n(buf.data(),
                         static_cast<std::iter_difference_t<char*>>(buf.size()),
                         format_str,
                         std::forward<VArgs>(args)...);
    return ErrPolicy::success(static_cast<std::size_t>(size));
  };

  return detail::fmt_structure_impl<ErrPolicy, std::size_t>(fmt_impl);
}

/**
 * @brief Unchecked version of fmt()
 */
export template <class... VArgs>
PF_PURE_FUNC [[nodiscard]] std::size_t
fmt_unchecked(std::span<char> buf,
              std::format_string<VArgs...> format_str,
              VArgs&&... args) {
  static constexpr std::string_view fail_msg = "format error";
  return fmt<ErrPolicyNothing<std::size_t, fail_msg>>(
      buf, format_str, std::forward<VArgs>(args)...);
}

/**
 * @brief Try version of fmt() that returns std::optional<std::size_t>
 *
 *@error_handling Catches all exceptions and returns std::nullopt instead
 */
export template <class... VArgs>
PF_PURE_FUNC [[nodiscard]] std::optional<std::size_t>
try_fmt(std::span<char> buf,
        std::format_string<VArgs...> format_str,
        VArgs&&... args) PF_NOEXCEPT {
  return fmt<ErrPolicyOptional<std::size_t>>(
      buf, format_str, std::forward<VArgs>(args)...);
}

/** *@brief Convenience overload of fmt() that formats into its own
 * FmtResult<BufLen> storage instead of a caller supplied buffer
 *
 *@tparam BufLen storage size in bytes
 */
export template <std::size_t BufLen,
                 typename ErrPolicy = ErrPolicyThrows<FmtResult<BufLen>, FmtError>,
                 class... VArgs>
  requires ErrPolicy_c<ErrPolicy, FmtResult<BufLen>> && requires(const char* str) {
    { ErrPolicy::fail(str) } -> std::same_as<typename ErrPolicy::ReturnType>;
  }
PF_PURE_FUNC [[nodiscard]] ErrPolicy::ReturnType
fmt(std::format_string<VArgs...> format_str, VArgs&&... args)
    PF_NOEXCEPT_COND(ErrPolicy::is_noexcept && !ErrPolicy::enabled) {
  FmtResult<BufLen> result;

  auto fmt_impl = [&]() {
    result.size = fmt_unchecked({result.str.data(), result.buffer_size},
                                format_str,
                                std::forward<VArgs>(args)...);
    return result;
  };

  return detail::fmt_structure_impl<ErrPolicy, FmtResult<BufLen>>(fmt_impl);
}

export template <std::size_t BufLen, class... VArgs>
PF_PURE_FUNC [[nodiscard]] FmtResult<BufLen>
fmt_unchecked(std::format_string<VArgs...> format_str, VArgs&&... args) {
  static constexpr std::string_view fail_msg = "format error";
  return fmt<BufLen, ErrPolicyNothing<FmtResult<BufLen>, fail_msg>>(
      format_str, std::forward<VArgs>(args)...);
}

export template <std::size_t BufLen, class... VArgs>
PF_PURE_FUNC [[nodiscard]] std::optional<FmtResult<BufLen>>
try_fmt(std::format_string<VArgs...> format_str, VArgs&&... args) PF_NOEXCEPT {
  return fmt<BufLen, ErrPolicyOptional<FmtResult<BufLen>>>(format_str,
                                                           std::forward<VArgs>(args)...);
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
export template <typename ErrPolicy = ErrPolicyThrows<std::size_t, FmtError>,
                 class... VArgs>
  requires ErrPolicy_c<ErrPolicy, std::size_t> && requires(const char* str) {
    { ErrPolicy::fail(str) } -> std::same_as<typename ErrPolicy::ReturnType>;
  }
PF_PURE_FUNC [[nodiscard]] ErrPolicy::ReturnType
fmt_cstr(std::span<char> buf, std::format_string<VArgs...> format_str, VArgs&&... args)
    PF_NOEXCEPT_COND(ErrPolicy::is_noexcept && !ErrPolicy::enabled) {
  PF_REQUIRE_ASSUME(format_str.get().size() < buf.size());

  auto fmt_impl = [&]() {
    [[maybe_unused]] auto [out, size] =
        std::format_to_n(buf.data(),
                         static_cast<std::iter_difference_t<char*>>(buf.size() - 1),
                         format_str,
                         std::forward<VArgs>(args)...);
    buf.data()[size] = '\0';
    return static_cast<std::size_t>(size);
  };

  return detail::fmt_structure_impl<ErrPolicy, std::size_t>(fmt_impl);
}

export template <class... VArgs>
PF_PURE_FUNC [[nodiscard]] std::size_t
fmt_cstr_unchecked(std::span<char> buf,
                   std::format_string<VArgs...> format_str,
                   VArgs&&... args) {
  PF_REQUIRE_ASSUME(format_str.get().size() < buf.size());
  static constexpr std::string_view msg = "fmt error";
  return fmt_cstr<ErrPolicyNothing<std::size_t, msg>>(
      buf, format_str, std::forward<VArgs>(args)...);
}

export template <class... VArgs>
PF_PURE_FUNC [[nodiscard]] std::optional<std::size_t>
try_fmt_cstr(std::span<char> buf,
             std::format_string<VArgs...> format_str,
             VArgs&&... args)
    PF_NOEXCEPT_COND(ErrPolicy::is_noexcept && !ErrPolicy::enabled) {
  PF_REQUIRE_ASSUME(format_str.get().size() < buf.size());
  return fmt_cstr<ErrPolicyOptional<std::size_t>>(
      buf, format_str, std::forward<VArgs>(args)...);
}

export template <std::size_t BufLen,
                 typename ErrPolicy = ErrPolicyThrows<FmtResult<BufLen>, FmtError>,
                 class... VArgs>
  requires ErrPolicy_c<ErrPolicy, FmtResult<BufLen>> && requires(const char* str) {
    { ErrPolicy::fail(str) } -> std::same_as<typename ErrPolicy::ReturnType>;
  }
PF_PURE_FUNC [[nodiscard]] ErrPolicy::ReturnType
fmt_cstr(std::format_string<VArgs...> format_str, VArgs&&... args)
    PF_NOEXCEPT_COND(ErrPolicy::is_noexcept && !ErrPolicy::enabled) {
  FmtResult<BufLen> result;

  auto fmt_impl = [&]() {
    result.size =
        fmt_cstr_unchecked(result.str, format_str, std::forward<VArgs>(args)...);
    return result;
  };

  return detail::fmt_structure_impl<ErrPolicy, FmtResult<BufLen>>(fmt_impl);
}

export template <std::size_t BufLen, class... VArgs>
PF_PURE_FUNC [[nodiscard]] FmtResult<BufLen>
fmt_cstr_unchecked(std::format_string<VArgs...> format_str, VArgs&&... args) {
  static constexpr std::string_view fail_msg = "format error";
  return fmt<BufLen, ErrPolicyNothing<FmtResult<BufLen>, fail_msg>>(
      format_str, std::forward<VArgs>(args)...);
}

export template <std::size_t BufLen, class... VArgs>
PF_PURE_FUNC [[nodiscard]] std::optional<FmtResult<BufLen>>
try_cstr_fmt(std::format_string<VArgs...> format_str, VArgs&&... args) PF_NOEXCEPT {
  return fmt<BufLen, ErrPolicyOptional<FmtResult<BufLen>>>(format_str,
                                                           std::forward<VArgs>(args)...);
}

}
