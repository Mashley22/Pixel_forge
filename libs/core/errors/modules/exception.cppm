module;

#include <stdexcept>
#ifdef PIXELFORGE_TEST
#include <print>
#endif

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:errors.exception;

import :require;
import :utils.strcpy;

#ifdef PIXELFORGE_TEST
#define DEBUG_PRINT(err_msg)     \
  do {                           \
    std::println("{}", err_msg); \
  } while (0)
#else
#define DEBUG_PRINT(err_str)
#endif

namespace pf {

/**
 *@brief Base class of all PixelForge exceptions
 *
 * Copies its message into a fixed-size static buffer (see msgBufSize) so
 * exceptions carry no dynamic allocations. Note that the buffer is shared
 * between all instances: constructing any Exception overwrites the message
 * seen by previously constructed ones. Extra per-exception data should be
 * stored in derived members, as InvalidCharToHexError does.
 */
export class Exception {
public:
  /**
   *@brief Default message buffer size in bytes, including null terminator
   */
  static constexpr std::size_t msgBufSizeDefault = 512;

  /**
   *@brief Active message buffer size in bytes; overridable at compile time
   * via PF_EXCEPTION_MSG_BUF_SIZE
   */
  static constexpr std::size_t msgBufSize =
#ifdef PF_EXCEPTION_MSG_BUF_SIZE
      PF_EXCEPTION_MSG_BUF_SIZE;
#else
      msgBufSizeDefault;
#endif
private:
  inline static thread_local std::array<char, msgBufSize> m_msgBuf{};

public:
  Exception() PF_NOEXCEPT = delete;

  /// Copies an existing exception's message unchanged
  constexpr Exception(const Exception&) PF_NOEXCEPT = default;

  /**
   *@brief Stores @p str truncated to the buffer size as the message
   */
  constexpr explicit Exception(const std::string_view& str) PF_NOEXCEPT {
    strcpy(m_msgBuf, str);
    DEBUG_PRINT(what());
  }

  /**
   *@brief Stores @p str truncated to the buffer size as the message
   */
  constexpr explicit Exception(const char* str) PF_NOEXCEPT {
    strcpy(m_msgBuf, {str, m_msgBuf.size()});
    DEBUG_PRINT(what());
  }

  /**
   *@brief Stores what() of @p error truncated to the buffer size
   */
  constexpr explicit Exception(const std::exception& error) PF_NOEXCEPT {
    strcpy(m_msgBuf, {error.what(), m_msgBuf.size()});
    DEBUG_PRINT(what());
  }

  /**
   *@brief The stored message, always null terminated
   */
  virtual constexpr const char*
  what() const PF_NOEXCEPT {
    return m_msgBuf.data();
  }
};

}
