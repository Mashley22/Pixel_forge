module;

#include <optional>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.meta.errPolicy;

export namespace pf {

/**
 *@brief ErrPolicy concept for non void types and the failure 
 *       should be cosntrained independently
 */
template<class Policy, typename T_result_type>
concept ErrPolicy = requires(T_result_type& successfulResult) {
/**
 *@brief whether a given policy introduces new exceptions that can be thrown
 *       i.e. optionals do not introduce new exceptions that can throw from within the function
 *
*/
  typename std::bool_constant<Policy::is_noexcept>;

  typename Policy::return_type;
  
  { Policy::success(successfulResult) };
  
  requires !std::is_same_v<void, T_result_type>;
};

template<class VoidPolicy>
concept VoidErrPolicy = requires() {
  typename std::bool_constant<VoidPolicy::is_noexcept>;

  typename VoidPolicy::return_type;
  
  { VoidPolicy::success() };
  
};

template<typename T_result_type>
struct ErrPolicy_nothing {
  static constexpr bool is_noexcept = true;

  using return_type = T_result_type;
    
  [[nodiscard]] static constexpr return_type
  success(T_result_type& successfulResult) PF_NOEXCEPT {
    return successfulResult;
  }

  template<class... V_args>
  static constexpr return_type
  fail(V_args... args) PF_NOEXCEPT {
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
  success(T_result_type& successfulResult) PF_NOEXCEPT {
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

static_assert(ErrPolicy<ErrPolicy_nothing<int>, int>);
static_assert(!ErrPolicy<ErrPolicy_nothing<void>, void>);
static_assert(VoidErrPolicy<ErrPolicy_nothing<void>>);

static_assert(ErrPolicy<ErrPolicy_optional<int>, int>);
static_assert(!ErrPolicy<ErrPolicy_optional<void>, void>);
static_assert(VoidErrPolicy<ErrPolicy_optional<void>>);

}
