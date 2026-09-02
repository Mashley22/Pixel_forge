module;

#include <optional>
#include <string_view>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:errors.errPolicy;

import :require;

export namespace pf {

/**
 *@brief Concept for policies returning a non-void result on success
 *
 *@note Fail is defined per use since that can get quite specific depending
 *      on different failure coniditions
 *
 *@tparam Policy the policy type
 *@tparam T_result_type value produced on the success path
 */
template <class Policy, typename T_result_type>
concept ErrPolicy_c = !std::is_same_v<T_result_type, void> &&
                      requires(const T_result_type& lvalue, T_result_type&& rvalue) {
                        /**
                         *@brief whether a given policy introduces new exceptions that can
                         * be thrown i.e. optionals do not introduce new exceptions that
                         * can throw from within the function
                         *
                         */
                        typename std::bool_constant<Policy::is_noexcept>;
                        typename std::bool_constant<Policy::enabled>;

                        typename Policy::return_type;

                        {
                          Policy::success(std::forward<T_result_type>(rvalue))
                        } -> std::same_as<typename Policy::return_type>;
                        {
                          Policy::success(static_cast<T_result_type&&>(rvalue))
                        } -> std::same_as<typename Policy::return_type>;

                        {
                          Policy::success(lvalue)
                        } -> std::same_as<typename Policy::return_type>;
                      };

/**
 *@brief Concept for policies whose success path carries no value
 *
 *@tparam VoidPolicy the policy type
 */
template <class VoidPolicy>
concept VoidErrPolicy_c = requires() {
  typename std::bool_constant<VoidPolicy::is_noexcept>;
  typename std::bool_constant<VoidPolicy::enabled>;

  typename VoidPolicy::return_type;

  { VoidPolicy::success() } -> std::same_as<typename VoidPolicy::return_type>;
};

/**
 *@brief Policy treating failure as unreachable: fail() traps via pf::require
 * in debug and is UB otherwise. Only use where the error path cannot occur.
 *
 *@note success returns the value unchanged, adding no overhead
 *
 *@tparam T_result_type value produced on the success path
 */
template <typename T_result_type, const std::string_view& T_fail_msg>
struct ErrPolicy_nothing {
  static constexpr bool is_noexcept = true;
  static constexpr bool enabled =
#ifdef NDEBUG
      false;
#else
      true;
#endif

  using return_type = T_result_type;

  [[nodiscard]] static constexpr return_type
  success(T_result_type&& successfulResult) PF_NOEXCEPT {
    return std::forward<T_result_type>(successfulResult);
  }

  [[nodiscard]] static constexpr return_type
  success(const T_result_type& successfulResult) PF_NOEXCEPT {
    return successfulResult;
  }

  static constexpr return_type
  fail([[maybe_unused]] const char* str) PF_NOEXCEPT {
    PF_REQUIRE(false, str);
    return return_type{};
  }

  template <class... V_args>
  static constexpr return_type
  fail([[maybe_unused]] V_args... args) PF_NOEXCEPT {
    PF_REQUIRE(false, T_fail_msg);
    return return_type{};
  }

};

/**
 *@brief Void specialisation of ErrPolicy_nothing for operations without a
 * meaningful result
 */
template <const std::string_view& T_fail_msg>
struct ErrPolicy_nothing<void, T_fail_msg> {
  static constexpr bool is_noexcept = true;
  static constexpr bool enabled =
#ifdef NDEBUG
      false;
#else
      true;
#endif

  using return_type = void;

  static constexpr void
  success() PF_NOEXCEPT {};

  template <class... V_args>
  static constexpr return_type
  fail(V_args... args) PF_NOEXCEPT {
    PF_REQUIRE(false, T_fail_msg);
    ((void) args, ...);
    return;
  }
};

/**
 *@brief Policy reporting failure as an empty std::optional instead of
 * throwing; introduces no new exceptions itself
 *
 *@tparam T_result_type element type of the returned optional
 */
template <typename T_result_type>
struct ErrPolicy_optional {
  static_assert(!std::is_same_v<T_result_type, void>, "A little silly");

  static constexpr bool is_noexcept = true;
  static constexpr bool enabled = true;
  using return_type = std::optional<T_result_type>;

  [[nodiscard]] static constexpr return_type
  success(T_result_type&& successfulResult) PF_NOEXCEPT {
    return std::make_optional(std::forward<T_result_type>(successfulResult));
  }

  [[nodiscard]] static constexpr return_type
  success(const T_result_type& successfulResult) PF_NOEXCEPT {
    return std::make_optional(successfulResult);
  }

  template <class... V_args>
  [[nodiscard]] static constexpr return_type
  fail(V_args... args) PF_NOEXCEPT {
    ((void) args, ...);
    return std::nullopt;
  }
};

/**
 *@brief Void specialisation of ErrPolicy_optional, maps success/failure onto
 * plain bool
 */
template <>
struct ErrPolicy_optional<void> {

  static constexpr bool is_noexcept = true;
  static constexpr bool enabled = true;
  using return_type = bool;

  [[nodiscard]] static constexpr return_type
  success() PF_NOEXCEPT {
    return true;
  };

  template <class... V_args>
  [[nodiscard]] static constexpr return_type
  fail(V_args... args) PF_NOEXCEPT {
    ((void) args, ...);
    return false;
  }
};

/**
 *@brief Policy throwing @p T_exception on failure
 *
 *@tparam T_result_type value produced on the success path
 *@tparam T_exception exception type thrown by fail(), forwarded any extra
 * arguments fail() received
 */
template <typename T_result_type, class T_exception>
struct ErrPolicy_throws {
  static constexpr bool is_noexcept = false;
  static constexpr bool enabled = true;
  using return_type = T_result_type;

  [[nodiscard]] static constexpr return_type
  success(T_result_type&& successfulResult) PF_NOEXCEPT {
    return std::forward<T_result_type>(successfulResult);
  }

  [[nodiscard]] static constexpr return_type
  success(const T_result_type& successfulResult) PF_NOEXCEPT {
    return successfulResult;
  }

  template <class... V_args>
  [[noreturn]] static constexpr return_type
  fail(V_args... args) {
    throw T_exception{std::forward<V_args>(args)...};
  }
};

/**
 *@brief Void specialisation of ErrPolicy_throws for operations without a
 * meaningful result
 *
 *@tparam T_exception exception type thrown by fail()
 */
template <class T_exception>
struct ErrPolicy_throws<void, T_exception> {
  static constexpr bool is_noexcept = false;
  static constexpr bool enabled = true;
  using return_type = void;

  static constexpr return_type
  success() PF_NOEXCEPT {}

  template <class... V_args>
  static constexpr return_type
  fail(V_args... args) {
    throw T_exception{std::forward<V_args>(args)...};
  }
};

}

namespace pf {

namespace {

constexpr std::string_view dummyStrView = "";
static_assert(ErrPolicy_c<ErrPolicy_nothing<int, dummyStrView>, int>);
static_assert(!ErrPolicy_c<ErrPolicy_nothing<void, dummyStrView>, void>);
static_assert(VoidErrPolicy_c<ErrPolicy_nothing<void, dummyStrView>>);

static_assert(ErrPolicy_c<ErrPolicy_optional<int>, int>);
static_assert(!ErrPolicy_c<ErrPolicy_optional<void>, void>);
static_assert(VoidErrPolicy_c<ErrPolicy_optional<void>>);

static_assert(ErrPolicy_c<ErrPolicy_throws<int, int>, int>);
static_assert(!ErrPolicy_c<ErrPolicy_throws<void, int>, void>);
static_assert(VoidErrPolicy_c<ErrPolicy_throws<void, int>>);

}

}
