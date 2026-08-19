module;

#include <stdexcept>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:errors.exception;

import :require;
import :utils.strcpy;

namespace pf {

export class Exception {
public:
  static constexpr std::size_t msgBufSizeDefault = 512;

  static constexpr std::size_t msgBufSize =
#ifdef PF_EXCEPTION_MSG_BUF_SIZE
      PF_EXCEPTION_MSG_BUF_SIZE;
#else
      msgBufSizeDefault;
#endif
private:
  static std::array<char, msgBufSize> m_msgBuf;

public:
  Exception() PF_NOEXCEPT = delete;
  constexpr Exception(const Exception&) PF_NOEXCEPT = default;
  constexpr explicit Exception(const std::string_view& str) PF_NOEXCEPT {
    strcpy(m_msgBuf, str);
  }
  constexpr explicit Exception(const char* str) PF_NOEXCEPT {
    strcpy(m_msgBuf, {str, m_msgBuf.size()});
  }
  constexpr explicit Exception(const std::exception& error) PF_NOEXCEPT {
    strcpy(m_msgBuf, {error.what(), m_msgBuf.size()});
  }

  constexpr const char*
  what() const PF_NOEXCEPT {
    return m_msgBuf.data();
  }
};

std::array<char, Exception::msgBufSize> Exception::m_msgBuf = {};

}
