module;

#include <optional>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.meta.errPolicy;

import PixelForge.core.require;

export namespace pf {

/**
 *@brief ErrPolicy concept for non void types and the failure 
 *       should be cosntrained independently
 */
template<class Policy, typename T_result_type>
concept ErrPolicy_c = !std::is_same_v<T_result_type, void> &&
  requires(const T_result_type& lvalue, T_result_type&& rvalue) {
/**
 *@brief whether a given policy introduces new exceptions that can be thrown
 *       i.e. optionals do not introduce new exceptions that can throw from within the function
 *
*/
  typename std::bool_constant<Policy::is_noexcept>;

  typename Policy::return_type;
  
  { Policy::success(std::forward<T_result_type>(rvalue)) } -> std::same_as<typename Policy::return_type>;
  { Policy::success(static_cast<T_result_type&&>(rvalue)) } -> std::same_as<typename Policy::return_type>;

  { Policy::success(lvalue) } -> std::same_as<typename Policy::return_type>;
};

template<class VoidPolicy>
concept VoidErrPolicy_c = requires() {
  typename std::bool_constant<VoidPolicy::is_noexcept>;

  typename VoidPolicy::return_type;
  
  { VoidPolicy::success() } -> std::same_as<typename VoidPolicy::return_type>;
};

template<typename T_result_type>
struct ErrPolicy_nothing {
  static constexpr bool is_noexcept = true;

  using return_type = T_result_type;
    
  [[nodiscard]] static constexpr return_type
  success(T_result_type&& successfulResult) PF_NOEXCEPT {
    return std::forward<T_result_type>(successfulResult);
  }

  [[nodiscard]] static constexpr return_type
  success(const T_result_type& successfulResult) PF_NOEXCEPT {
    return successfulResult;
  }

  template<class... V_args>
  static constexpr return_type
  fail(V_args... args) PF_NOEXCEPT {
    PF_REQUIRE(false);
    ((void)args, ...);
    return return_type{};
  }
};

template<>
struct ErrPolicy_nothing<void> {
  static constexpr bool is_noexcept = true;

  using return_type = void;

  static constexpr void
  success(void) PF_NOEXCEPT {};

  template<class... V_args>
  static constexpr return_type
  fail(V_args... args) PF_NOEXCEPT {
    ((void)args, ...);
    return return_type{};
  }
};

template<typename T_result_type>
struct ErrPolicy_optional {
  static_assert(!std::is_same_v<T_result_type, void>, "A little silly");

  static constexpr bool is_noexcept = true;
  using return_type = std::optional<T_result_type>;

  [[nodiscard]] static constexpr return_type
  success(T_result_type&& successfulResult) PF_NOEXCEPT {
    return std::make_optional(std::forward<T_result_type>(successfulResult));
  }

  [[nodiscard]] static constexpr return_type
  success(const T_result_type& successfulResult) PF_NOEXCEPT {
    return std::make_optional(successfulResult);
  }
  
  template<class... V_args>
  [[nodiscard]] static constexpr return_type
  fail(V_args... args) PF_NOEXCEPT {
    ((void)args, ...);
    return std::nullopt;
  }
};

template<>
struct ErrPolicy_optional<void> {

  static constexpr bool is_noexcept = true;
  using return_type = bool;

  [[nodiscard]] static constexpr return_type
  success(void) PF_NOEXCEPT {
    return true;
  };

  template<class... V_args>
  [[nodiscard]] static constexpr return_type
  fail(V_args... args) PF_NOEXCEPT {
    ((void)args, ...);
    return false;
  }
};

template<typename T_result_type, class T_exception>
struct ErrPolicy_throws {
  static constexpr bool is_noexcept = false;
  using return_type = T_result_type;
  
  [[nodiscard]] static constexpr return_type
  success(T_result_type&& successfulResult) PF_NOEXCEPT {
    return std::forward<T_result_type>(successfulResult);
  }

  [[nodiscard]] static constexpr return_type
  success(const T_result_type& successfulResult) PF_NOEXCEPT {
    return successfulResult;
  }
  
  template<class... V_args>
  [[noreturn]] static constexpr return_type
  fail(V_args... args) {
    throw T_exception{std::forward<V_args>(args)...};
  }
};

template<class T_exception>
struct ErrPolicy_throws<void, T_exception> {
  static constexpr bool is_noexcept = false;
  using return_type = void;
  
  static constexpr return_type
  success(void) PF_NOEXCEPT {}
  
  template<class... V_args>
  static constexpr return_type
  fail(V_args... args) {
    throw T_exception{std::forward<V_args>(args)...};
  }
};

static_assert(ErrPolicy_c<ErrPolicy_nothing<int>, int>);
static_assert(!ErrPolicy_c<ErrPolicy_nothing<void>, void>);
static_assert(VoidErrPolicy_c<ErrPolicy_nothing<void>>);

static_assert(ErrPolicy_c<ErrPolicy_optional<int>, int>);
static_assert(!ErrPolicy_c<ErrPolicy_optional<void>, void>);
static_assert(VoidErrPolicy_c<ErrPolicy_optional<void>>);

static_assert(ErrPolicy_c<ErrPolicy_throws<int, int>, int>);
static_assert(!ErrPolicy_c<ErrPolicy_throws<void, int>, void>);
static_assert(VoidErrPolicy_c<ErrPolicy_throws<void, int>>);

}
