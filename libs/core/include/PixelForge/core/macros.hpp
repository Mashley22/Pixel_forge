#ifndef PIXELFORGE_CORE_MACROS_HPP
#define PIXELFORGE_CORE_MACROS_HPP

#include <new>

#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  #define PF_NOEXCEPT 
#else
  #define PF_NOEXCEPT noexcept
#endif

#define PF_CACHE_LINE_ALIGN alignas(std::hardware_destructive_interference_size)

#define PF_REQUIRE(...) \
  PF_REQUIRE_IMPL(__VA_ARGS__, \
    PF_REQUIRE_IMPL_EXPR_MSG_FAIL_HANDLER, \
    PF_REQUIRE_IMPL_EXPR_MSG, \
    PF_REQUIRE_IMPL_EXPR_ONLY)(__VA_ARGS__)

#define PF_REQUIRE_ASSUME(...) \
  PF_REQUIRE_ASSUME_IMPL(__VA_ARGS__, \
    PF_REQUIRE_ASSUME_IMPL_EXPR_MSG_FAIL_HANDLER, \
    PF_REQUIRE_ASSUME_IMPL_EXPR_MSG, \
    PF_REQUIRE_ASSUME_IMPL_EXPR_ONLY)(__VA_ARGS__)

#define PF_REQUIRE_IMPL(_1, _2, _3, NAME, ...) NAME
#define PF_REQUIRE_ASSUME_IMPL(_1, _2, _3, NAME, ...) NAME

#ifdef PF_SUBMODULE_REQUIRE_FAIL_HANDLER
  #define PF_REQUIRE_IMPL_EXPR_ONLY(expr) \
    pf::require<PF_SUBMODULE_REQUIRE_FAIL_HANDLER>(expr)
  
  #define PF_REQUIRE_IMPL_EXPR_MSG(expr, msg) \
    pf::require<PF_SUBMODULE_REQUIRE_FAIL_HANDLER>(expr, msg)
#elif defined(PF_MODULE_REQUIRE_FAIL_HANDLER)
  #define PF_REQUIRE_IMPL_EXPR_ONLY(expr) \
    pf::require<PF_MODULE_REQUIRE_FAIL_HANDLER>(expr)
  
  #define PF_REQUIRE_IMPL_EXPR_MSG(expr, msg) \
    pf::require<PF_MODULE_REQUIRE_FAIL_HANDLER>(expr, msg)
#elif defined(PF_GLOBAL_REQUIRE_FAIL_HANDLER)
  #define PF_REQUIRE_IMPL_EXPR_ONLY(expr) \
    pf::require<PF_GLOBAL_REQUIRE_FAIL_HANDLER>(expr)
  
  #define PF_REQUIRE_IMPL_EXPR_MSG(expr, msg) \
    pf::require<PF_GLOBAL_REQUIRE_FAIL_HANDLER>(expr, msg)
#else
  #define PF_REQUIRE_IMPL_EXPR_ONLY(expr) \
    pf::require(expr)
  
  #define PF_REQUIRE_IMPL_EXPR_MSG(expr, msg) \
    pf::require(expr, msg)
#endif /* PF_SUBMODULE_REQUIRE_FAIL_HANDLER */

#define PF_REQUIRE_IMPL_EXPR_MSG_FAIL_HANDLER(expr, msg, fail_handler) \
  pf::require<fail_handler>(expr, msg)

// REQUIRE_ASSUME implementations (with [[assume]])
#define PF_REQUIRE_ASSUME_IMPL_EXPR_ONLY(expr) \
  do { \
    PF_REQUIRE_IMPL_EXPR_ONLY(expr); \
    [[assume(expr)]]; \
  } while(0)

#define PF_REQUIRE_ASSUME_IMPL_EXPR_MSG(expr, msg) \
  do { \
    PF_REQUIRE_IMPL_EXPR_MSG(expr, msg); \
    [[assume(expr)]]; \
  } while(0)

#define PF_REQUIRE_ASSUME_IMPL_EXPR_MSG_FAIL_HANDLER(expr, msg, fail_handler) \
  do { \
    PF_REQUIRE_IMPL_EXPR_MSG_FAIL_HANDLER(expr, msg, fail_handler); \
    [[assume(expr)]]; \
  } while(0)

#endif /* PIXELFORGE_CORE_MACROS_HPP */
