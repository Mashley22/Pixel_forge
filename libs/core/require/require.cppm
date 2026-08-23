module;

#include <array>
#include <chrono>
#include <source_location>
#include <string_view>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:require;

export namespace pf {

/**
 *@brief Payload describing a failed requirement, thrown by
 * RequireFail_throw and usable as a custom exception type
 */
struct RequireFail {
  std::string_view msg;
  std::source_location loc;
};

/**
 *@brief Concept for policies handling failed requirements
 *
 * A fail handler exposes a static fail() that receives the failure message
 * and call site. It may return (log-and-continue), terminate or throw.
 *
 *@tparam T the handler type
 */
template <typename T>
concept RequireFailHandler_c =
    requires(const std::string_view msg, const std::source_location loc) {
      { T::fail(msg, loc) } -> std::same_as<void>;
    };

/**
 *@brief Fail policy that silently ignores failures, execution continues
 */
struct RequireFail_doNothing {
  constexpr static void
  fail(const std::string_view msg, const std::source_location loc) PF_NOEXCEPT {
    (void) msg;
    (void) loc;
  }
};

/**
 *@brief Fail policy that terminates the program on failure
 */
struct RequireFail_terminate {
  [[noreturn]] constexpr static void
  fail(const std::string_view msg, const std::source_location loc) PF_NOEXCEPT {
    (void) msg;
    (void) loc;
    std::terminate();
  }
};

/**
 *@note BE CAREFUL, if using this make sure to
 * PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
 */
struct RequireFail_throw {
  [[noreturn]] constexpr static void
  fail(const std::string_view msg, const std::source_location loc) {
    throw RequireFail{.msg = msg, .loc = loc};
  }
};

/**
 *@brief Snapshot of a single failed requirement
 */
struct RequireFailInfo {
  std::string_view msg{nullptr, 0};
  std::source_location loc;
  std::time_t time =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  /**
   *@brief Checks whether this info carries no message
   *
   *@return true if no message was recorded
   */
  [[nodiscard]] bool
  empty() const PF_NOEXCEPT;
};

/**
 *@brief Failure policy keeping a small per-thread circular log of recent
 * failures instead of aborting, so several can be inspected after the fact
 *
 * The buffer holds PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE entries; once full,
 * the oldest entry is overwritten.
 */
struct RequireFail_logContinue {
  /**
   *@brief Records the failure into the circular buffer and returns
   */
  static void
  fail(std::string_view msg, std::source_location loc) PF_NOEXCEPT;

  /**
   *@brief Index where the next failure will be written
   */
  [[nodiscard]] static std::size_t
  currentIdx() PF_NOEXCEPT;

  /**
   *@brief The raw circular log storage, oldest entries may sit past
   * currentIdx()
   */
  [[nodiscard]] static std::span<RequireFailInfo, PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE>
  failInfos() PF_NOEXCEPT;

  /**
   *@brief The most recently recorded failure
   */
  [[nodiscard]] static const RequireFailInfo&
  getLastError() PF_NOEXCEPT;

private:
  static thread_local std::array<RequireFailInfo, PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE>
      m_failInfos;
  static thread_local std::size_t m_currentIdx;
};

/**
 *@brief Failure policy that logs a single failure then terminates
 */
struct RequireFail_logTerminate {
  /**
   *@brief Records the failure into m_failInfo and terminates
   */
  [[noreturn]] static void
  fail(std::string_view msg, std::source_location loc);

  /**
   *@brief The recorded failure info of the last (terminating) failure
   */
  const RequireFailInfo&
  failInfo() PF_NOEXCEPT;

private:
  static RequireFailInfo m_failInfo;
};

/**
 *@brief Runtime assertion. If @p expr evaluates to false the configured fail
 * policy is invoked with @p msg and @p location
 *
 *@tparam RequireFail_policy handler invoked on failure, defaults to
 * RequireFail_terminate
 *@param expr condition that must hold
 *@param msg optional diagnostic forwarded to the handler
 *@param location call site, captured automatically via source_location
 *
 *@note when compiled with PIXELFORGE_REQUIRE_THROWS_ON_FAILURE (validation
 * builds) the policy is bypassed and RequireFail_throw::fail() throws a
 * pf::RequireFail on every failure
 */
template <RequireFailHandler_c RequireFail_policy = RequireFail_terminate>
constexpr void
require(const bool expr,
        const std::string_view msg = {},
        const std::source_location location = std::source_location::current()) {
  if (!expr) {
#ifdef PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
    RequireFail_throw::fail(msg, location);
#else
    RequireFail_policy::fail(msg, location);
#endif
  }
}

}
