module;

#include <atomic>
#include <array>
#include <chrono>
#include <source_location>
#include <string_view>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.require;

export namespace pf {

struct RequireFail {
  std::string_view msg;
  std::source_location loc;
};

template<typename T>
concept RequireFailHandler_c = requires(const std::string_view msg, const std::source_location loc) {
  { T::fail(msg, loc) } -> std::same_as<void>;
};

struct RequireFail_doNothing {
  static void
  fail(const std::string_view msg, const std::source_location loc);
};

struct RequireFail_terminate {
  [[noreturn]] static void
  fail(const std::string_view msg, const std::source_location loc);
};

/**
 *@note BE CAREFUL, if using this make sure to PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
 */
struct RequireFail_throw {
  [[noreturn]] static void
  fail(const std::string_view msg, const std::source_location loc);
};

struct RequireFailInfo {
  std::string_view msg{nullptr, 0};
  std::source_location loc;
  std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  [[nodiscard]] bool
  empty(void) PF_NOEXCEPT;
};

/**@brief this uses a small circular logging buffer
  */
struct RequireFail_logContinue {
  static void
  fail(const std::string_view msg, const std::source_location loc);

  [[nodiscard]] static std::size_t 
  currentIdx(void) PF_NOEXCEPT;

  [[nodiscard]] static std::span<RequireFailInfo, PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE>
  failInfos(void) PF_NOEXCEPT;

private:
  static std::array<RequireFailInfo, PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE> m_failInfos;
  static std::atomic<std::size_t> m_currentIdx;
};

struct RequireFail_logTerminate {
  [[noreturn]] static void
  fail(const std::string_view msg, const std::source_location loc);

  const RequireFailInfo& 
  failInfo(void) PF_NOEXCEPT;

private:
  static RequireFailInfo m_failInfo;
};

template<RequireFailHandler_c RequireFailPolicy = RequireFail_terminate>
void
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
