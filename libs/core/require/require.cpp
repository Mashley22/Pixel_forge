module;

#include <exception>
#include <limits>
#include <source_location>
#include <span>
#include <string_view>

#include <PixelForge/core/macros.hpp>

module PixelForge.core;

namespace pf {

RequireFailInfo RequireFail_logTerminate::m_failInfo{};

thread_local std::array<RequireFailInfo, PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE>
  RequireFail_logContinue::m_failInfos{};

thread_local std::size_t RequireFail_logContinue::m_currentIdx =
  std::numeric_limits<std::size_t>::max();

void
RequireFail_logTerminate::fail(const std::string_view msg, const std::source_location loc) {
  m_failInfo = {.msg = msg, .loc = loc};
  std::terminate();
}

const RequireFailInfo&
RequireFail_logTerminate::failInfo() PF_NOEXCEPT {
  return m_failInfo;
}

std::size_t
RequireFail_logContinue::currentIdx() PF_NOEXCEPT {
  return m_currentIdx;
}

const RequireFailInfo&
RequireFail_logContinue::getLastError() PF_NOEXCEPT {
  return m_failInfos[m_currentIdx % m_failInfos.size()];
}

bool
RequireFailInfo::empty() const PF_NOEXCEPT {
  return msg.empty();
}

std::span<RequireFailInfo, PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE>
RequireFail_logContinue::failInfos() PF_NOEXCEPT {
  return m_failInfos;
}

void
RequireFail_logContinue::fail(const std::string_view msg,
                              const std::source_location loc) PF_NOEXCEPT {
  m_failInfos[(++m_currentIdx) % m_failInfos.size()] = {.msg = msg, .loc = loc};
}

}
