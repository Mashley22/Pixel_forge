module;

#include <atomic>
#include <exception>
#include <source_location>
#include <string_view>
#include <span>

#include <PixelForge/core/macros.hpp>

module PixelForge.core.require;

namespace pf {

RequireFailInfo RequireFail_logTerminate::m_failInfo{};

std::array<RequireFailInfo, PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE>
RequireFail_logContinue::m_failInfos{};

std::atomic<std::size_t>
RequireFail_logContinue::m_currentIdx = 0;

void 
RequireFail_terminate::fail(const std::string_view msg,
                            const std::source_location loc) {
  (void)msg;
  (void)loc;
  std::terminate();
}

void
RequireFail_doNothing::fail(const std::string_view msg,
                            const std::source_location loc) {
  (void)msg;
  (void)loc;
}

void
RequireFail_throw::fail(const std::string_view msg,
                        const std::source_location loc) {
  throw RequireFail{.msg = msg,
                    .loc = loc};
}

void
RequireFail_logTerminate::fail(const std::string_view msg,
                               const std::source_location loc) {
  m_failInfo = {
    .msg = msg, 
    .loc = loc
  };
  std::terminate();          
}

const RequireFailInfo&
RequireFail_logTerminate::failInfo(void) PF_NOEXCEPT {
  return m_failInfo;
}

std::size_t
RequireFail_logContinue::currentIdx(void) PF_NOEXCEPT {
  return m_currentIdx;
}

std::span<RequireFailInfo, PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE>
RequireFail_logContinue::failInfos(void) PF_NOEXCEPT {
  return m_failInfos;
}

void
RequireFail_logContinue::fail(const std::string_view msg,
                              const std::source_location loc) PF_NOEXCEPT {
  m_failInfos[m_currentIdx++] = {.msg = msg, .loc = loc};
}

}
