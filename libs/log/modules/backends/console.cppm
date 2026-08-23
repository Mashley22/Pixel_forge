module;

#include <array>
#include <chrono>
#include <concepts>
#include <iostream>
#include <mutex>
#include <span>

#include <PixelForge/core/macros.hpp>

export module PixelForge.logging:backend.console;

import :record;

import PixelForge.core;

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
   *@note thread-safe: formatting runs on purely local storage, and the
   * single stream insertion is guarded by m_lock so concurrent producers
   * cannot interleave within each other's lines. Records may still
   * interleave at line granularity, and between std::cout and std::cerr.
   */
  static void
  log(const Record& record) PF_NOEXCEPT {
    std::array<char, lineBufSize> buf{};

    const std::size_t size = formatLine(record, buf);
    std::size_t len = (size < buf.size()) ? size : buf.size() - 1;
    buf[len++] = '\n';

    std::ostream& stream = (record.level >= Level::WARNING) ? std::cerr : std::cout;
    const std::lock_guard<std::mutex> lock(m_lock);
    stream.write(buf.data(), static_cast<std::streamsize>(len));
  }
};

static_assert(Backend_c<ConsoleBackend>, "ConsoleBackend must satisfy Backend_c");

}
