module;

#include <chrono>
#include <concepts>
#include <cstdint>
#include <format>
#include <string_view>

#include <PixelForge/core/macros.hpp>

export module PixelForge.logging:record;

export namespace pf::log {

#define PF_DEFAULT_LOG_LEVEL_LIST          \
  PF_LOG_LEVEL_X_MACRO(DEBUG, 10, "DBG")   \
  PF_LOG_LEVEL_X_MACRO(INFO, 20, "INF")    \
  PF_LOG_LEVEL_X_MACRO(WARNING, 30, "WRN") \
  PF_LOG_LEVEL_X_MACRO(ERROR, 40, "ERR")

using Level_t = std::uint8_t;

using LogIdentifier = std::uint8_t;

enum class Level : Level_t {
#define PF_LOG_LEVEL_X_MACRO(name, val, str) name = (val),
  PF_DEFAULT_LOG_LEVEL_LIST
#undef PF_LOG_LEVEL_X_MACRO
};

[[nodiscard]] constexpr const char*
toStr(const Level& lvl) PF_NOEXCEPT {
#define PF_LOG_LEVEL_X_MACRO(name, val, str) \
  case Level::name: return str;

  switch (lvl) {
    PF_DEFAULT_LOG_LEVEL_LIST
  default:
    return "UKN"; // need to change this at some point
  }
#undef PF_LOG_LEVEL_X_MACRO
}

struct Record {
  std::chrono::system_clock::time_point time;
  Level level;
  LogIdentifier id;
  std::string_view msg;
};

}

template <>
struct std::formatter<pf::log::Level> : std::formatter<std::string_view> {
  auto
  format(pf::log::Level level, std::format_context& ctx) const {
    return std::formatter<std::string_view>::format(pf::log::toStr(level), ctx);
  }
};

export namespace pf::log {

constexpr std::format_string<std::chrono::system_clock::time_point,
                             Level,
                             LogIdentifier,
                             std::string_view>
   defaultFormat = "[{:%Y-%m-%d %H:%M:%S}] [{:>3}] [thread-{:02X}] {}";

template <class LoggerBackend>
concept Backend_c = requires(const Record& record) {
  { LoggerBackend::log(record) } -> std::same_as<void>;
};

}
