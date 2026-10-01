module;

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
 * Copies its message into a fixed-size static buffer (see msg_buf_size) so
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
  static constexpr std::size_t msg_buf_size_default = 512;

  /**
   *@brief Active message buffer size in bytes; overridable at compile time
   * via PF_EXCEPTION_MSG_BUF_SIZE
   */
  static constexpr std::size_t msg_buf_size =
#ifdef PF_EXCEPTION_MSG_BUF_SIZE
      PF_EXCEPTION_MSG_BUF_SIZE;
#else
      msg_buf_size_default;
#endif
private:
  inline static thread_local std::array<char, msg_buf_size> m_msg_buf{};

public:
  Exception() PF_NOEXCEPT = delete;

  /**
   *@brief Stores @p str truncated to the buffer size as the message
   */
  constexpr explicit Exception(const std::string_view& str) PF_NOEXCEPT {
    strcpy(m_msg_buf, str);
    DEBUG_PRINT(what());
  }

  /**
   *@brief Stores @p str truncated to the buffer size as the message
   */
  constexpr explicit Exception(const char* str) PF_NOEXCEPT {
    strcpy(m_msg_buf, {str, m_msg_buf.size()});
    DEBUG_PRINT(what());
  }

  /**
   *@brief Stores what() of @p error truncated to the buffer size
   */
  constexpr explicit Exception(const std::exception& error) PF_NOEXCEPT {
    strcpy(m_msg_buf, {error.what(), m_msg_buf.size()});
    DEBUG_PRINT(what());
  }

  /**
   *@brief The stored message, always null terminated
   */
  virtual constexpr const char*
  what() const PF_NOEXCEPT {
    return m_msg_buf.data();
  }
};

}
