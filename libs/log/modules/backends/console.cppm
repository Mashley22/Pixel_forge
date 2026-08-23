module;

#include <array>
#include <chrono>
#include <concepts>
#include <cstdio>
#include <mutex>
#include <span>

#include <PixelForge/core/macros.hpp>

export module PixelForge.logging:backend.console;

import :record;

import PixelForge.core;

// -Wstrict-overflow=5 fires inside libstdc++'s chrono formatter
// (__formatter_chrono::_M_D_x) instantiated by formatLine(); GCC documents
// level 5 as prone to false positives. The diagnostic is emitted at
// end-of-TU, so it cannot be scoped around the call site.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#endif

export namespace pf::log {

/**
 *@brief Logging backend writing formatted records to the process console
 *
 * WARNING and above go to stderr (flushed immediately), everything else to
 * stdout.
 *
 *@note satisfies Backend_c
 */
struct ConsoleBackend {
private:
  /**
   *@brief Serialises line emission between producers so records never
   * interleave mid-line
   */
  inline static std::mutex m_lock{};

public:
  /**
   *@brief Stack space reserved per logged line; longer messages truncate
   */
  static constexpr std::size_t lineBufSize = 512;

  /**
   *@brief Renders a record into a single console line following
   * defaultFormat
   *
   *@param record the record to render
   *@param buf destination buffer; output is truncated to fit and NOT null
   * terminated
   *
   *@return number of characters the FULL line occupies, which may exceed
   * buf.size(); exactly min(return value, buf.size()) bytes were written
   */
  [[nodiscard]] static std::size_t
  formatLine(const Record& record, std::span<char> buf) PF_NOEXCEPT {
    return fmt(buf, defaultFormat, record.time, record.level, record.id, record.msg);
  }

  /**
   *@brief Formats @p record with formatLine() and writes it followed by a
   * newline
   *
   *@param record the record to log
   *
   *@note thread-safe: formatting runs on purely local storage, and line
   * emission is guarded by m_lock so concurrent producers cannot interleave
   * within each other's lines. Records may still interleave at line
   * granularity, and between stdout and stderr.
   */
  static void
  log(const Record& record) PF_NOEXCEPT {
    std::array<char, lineBufSize> buf{};

    const std::size_t size = formatLine(record, buf);
    const std::size_t written = (size < buf.size()) ? size : buf.size();

    std::FILE* stream = (record.level >= Level::WARNING) ? stderr : stdout;
    {
      std::lock_guard<std::mutex> lock(m_lock);
      (void) std::fwrite(buf.data(), 1, written, stream);
      (void) std::fputc('\n', stream);
    }

    if (stream == stderr) {
      (void) std::fflush(stream);
    }
  }
};

static_assert(Backend_c<ConsoleBackend>, "ConsoleBackend must satisfy Backend_c");

}
