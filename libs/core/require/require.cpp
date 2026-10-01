module;

#include <exception>
#include <limits>
#include <source_location>
#include <span>
#include <string_view>

#include <PixelForge/core/macros.hpp>

module PixelForge.core;

namespace pf {

RequireFailInfo RequireFailLogTerminate::m_fail_info{};

thread_local std::array<RequireFailInfo, PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE>
    RequireFailLogContinue::m_fail_infos{};

thread_local std::size_t RequireFailLogContinue::m_current_idx =
    std::numeric_limits<std::size_t>::max();

void
RequireFailLogTerminate::fail(const std::string_view msg,
                              const std::source_location loc) {
  m_fail_info = {.msg = msg, .loc = loc};
  std::terminate();
}

const RequireFailInfo&
RequireFailLogTerminate::fail_info() PF_NOEXCEPT {
  return m_fail_info;
}

std::size_t
RequireFailLogContinue::current_idx() PF_NOEXCEPT {
  return m_current_idx;
}

const RequireFailInfo&
RequireFailLogContinue::get_last_error() PF_NOEXCEPT {
  return m_fail_infos[m_current_idx % m_fail_infos.size()];
}

bool
RequireFailInfo::empty() const PF_NOEXCEPT {
  return msg.empty();
}

std::span<RequireFailInfo, PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE>
RequireFailLogContinue::fail_infos() PF_NOEXCEPT {
  return m_fail_infos;
}

void
RequireFailLogContinue::fail(const std::string_view msg,
                             const std::source_location loc) PF_NOEXCEPT {
  m_fail_infos[(++m_current_idx) % m_fail_infos.size()] = {.msg = msg, .loc = loc};
}

}
