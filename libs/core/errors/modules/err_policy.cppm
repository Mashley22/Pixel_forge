module;

#include <optional>
#include <string_view>
#include <utility>

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
 *@tparam ResultType value produced on the success path
 */
template <class Policy, typename ResultType>
concept ErrPolicy_c = !std::is_same_v<ResultType, void> &&
                      requires(const ResultType& lvalue, ResultType&& rvalue) {
                        /**
                         *@brief whether a given policy introduces new exceptions that can
                         * be thrown i.e. optionals do not introduce new exceptions that
                         * can throw from within the function
                         *
                         */
                        typename std::bool_constant<Policy::is_noexcept>;
                        typename std::bool_constant<Policy::enabled>;

                        typename Policy::ReturnType;

                        {
                          Policy::success(std::forward<ResultType>(rvalue))
                        } -> std::same_as<typename Policy::ReturnType>;
                        {
                          Policy::success(static_cast<ResultType&&>(rvalue))
                        } -> std::same_as<typename Policy::ReturnType>;

                        {
                          Policy::success(lvalue)
                        } -> std::same_as<typename Policy::ReturnType>;
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

  typename VoidPolicy::ReturnType;

  { VoidPolicy::success() } -> std::same_as<typename VoidPolicy::ReturnType>;
};

/**
 *@brief Policy treating failure as unreachable: fail() traps via pf::require
 * in debug and is UB otherwise. Only use where the error path cannot occur.
 *
 *@note success returns the value unchanged, adding no overhead
 *
 *@tparam ResultType value produced on the success path
 */
template <typename ResultType, const std::string_view& FailMsg>
struct ErrPolicyNothing {
  static constexpr bool is_noexcept = true;
  static constexpr bool enabled =
#ifdef NDEBUG
      false;
#else
      true;
#endif

  using ReturnType = ResultType;

  [[nodiscard]] static constexpr ReturnType
  success(ResultType&& successful_result) PF_NOEXCEPT {
    return std::forward<ResultType>(successful_result);
  }

  [[nodiscard]] static constexpr ReturnType
  success(const ResultType& successful_result) PF_NOEXCEPT {
    return successful_result;
  }

  [[noreturn]] static constexpr ReturnType
  fail([[maybe_unused]] const char* str) PF_NOEXCEPT {
    PF_REQUIRE(false, str);
    std::unreachable();
  }

  template <class... VArgs>
  [[noreturn]] static constexpr ReturnType
  fail([[maybe_unused]] VArgs... args) PF_NOEXCEPT {
    PF_REQUIRE(false, FailMsg);
    std::unreachable();
  }
};

/**
 *@brief Void specialisation of ErrPolicyNothing for operations without a
 * meaningful result
 */
template <const std::string_view& FailMsg>
struct ErrPolicyNothing<void, FailMsg> {
  static constexpr bool is_noexcept = true;
  static constexpr bool enabled =
#ifdef NDEBUG
      false;
#else
      true;
#endif

  using ReturnType = void;

  static constexpr void
  success() PF_NOEXCEPT {};

  template <class... VArgs>
  static constexpr ReturnType
  fail(VArgs... args) PF_NOEXCEPT {
    PF_REQUIRE(false, FailMsg);
    ((void) args, ...);
    return;
  }
};

/**
 *@brief Policy reporting failure as an empty std::optional instead of
 * throwing; introduces no new exceptions itself
 *
 *@tparam ResultType element type of the returned optional
 */
template <typename ResultType>
struct ErrPolicyOptional {
  static_assert(!std::is_same_v<ResultType, void>, "A little silly");

  static constexpr bool is_noexcept = true;
  static constexpr bool enabled = true;
  using ReturnType = std::optional<ResultType>;

  [[nodiscard]] static constexpr ReturnType
  success(ResultType&& successful_result) PF_NOEXCEPT {
    return std::make_optional(std::forward<ResultType>(successful_result));
  }

  [[nodiscard]] static constexpr ReturnType
  success(const ResultType& successful_result) PF_NOEXCEPT {
    return std::make_optional(successful_result);
  }

  template <class... VArgs>
  [[nodiscard]] static constexpr ReturnType
  fail(VArgs... args) PF_NOEXCEPT {
    ((void) args, ...);
    return std::nullopt;
  }
};

/**
 *@brief Void specialisation of ErrPolicyOptional, maps success/failure onto
 * plain bool
 */
template <>
struct ErrPolicyOptional<void> {

  static constexpr bool is_noexcept = true;
  static constexpr bool enabled = true;
  using ReturnType = bool;

  [[nodiscard]] static constexpr ReturnType
  success() PF_NOEXCEPT {
    return true;
  };

  template <class... VArgs>
  [[nodiscard]] static constexpr ReturnType
  fail(VArgs... args) PF_NOEXCEPT {
    ((void) args, ...);
    return false;
  }
};

/**
 *@brief Policy throwing @p ExceptionT on failure
 *
 *@tparam ResultType value produced on the success path
 *@tparam ExceptionT exception type thrown by fail(), forwarded any extra
 * arguments fail() received
 */
template <typename ResultType, class ExceptionT>
struct ErrPolicyThrows {
  static constexpr bool is_noexcept = false;
  static constexpr bool enabled = true;
  using ReturnType = ResultType;

  [[nodiscard]] static constexpr ReturnType
  success(ResultType&& successful_result) PF_NOEXCEPT {
    return std::forward<ResultType>(successful_result);
  }

  [[nodiscard]] static constexpr ReturnType
  success(const ResultType& successful_result) PF_NOEXCEPT {
    return successful_result;
  }

  template <class... VArgs>
  [[noreturn]] static constexpr ReturnType
  fail(VArgs... args) {
    throw ExceptionT{std::forward<VArgs>(args)...};
  }
};

/**
 *@brief Void specialisation of ErrPolicyThrows for operations without a
 * meaningful result
 *
 *@tparam ExceptionT exception type thrown by fail()
 */
template <class ExceptionT>
struct ErrPolicyThrows<void, ExceptionT> {
  static constexpr bool is_noexcept = false;
  static constexpr bool enabled = true;
  using ReturnType = void;

  static constexpr ReturnType
  success() PF_NOEXCEPT {}

  template <class... VArgs>
  static constexpr ReturnType
  fail(VArgs... args) {
    throw ExceptionT{std::forward<VArgs>(args)...};
  }
};

}

namespace pf {

namespace {

constexpr std::string_view dummy_str_view = "";
static_assert(ErrPolicy_c<ErrPolicyNothing<int, dummy_str_view>, int>);
static_assert(!ErrPolicy_c<ErrPolicyNothing<void, dummy_str_view>, void>);
static_assert(VoidErrPolicy_c<ErrPolicyNothing<void, dummy_str_view>>);

static_assert(ErrPolicy_c<ErrPolicyOptional<int>, int>);
static_assert(!ErrPolicy_c<ErrPolicyOptional<void>, void>);
static_assert(VoidErrPolicy_c<ErrPolicyOptional<void>>);

static_assert(ErrPolicy_c<ErrPolicyThrows<int, int>, int>);
static_assert(!ErrPolicy_c<ErrPolicyThrows<void, int>, void>);
static_assert(VoidErrPolicy_c<ErrPolicyThrows<void, int>>);

}

}
