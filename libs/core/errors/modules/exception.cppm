module;

#include <stdexcept>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:errors.exception;

import PixelForge.core.require;

namespace pf {

export
class Exception {
public:
  static constexpr std::size_t msgBufSizeDefault = 512;

  static constexpr std::size_t msgBufSize = 
#ifdef PF_EXCEPTION_MSG_BUF_SIZE
  PF_EXCEPTION_MSG_BUF_SIZE;
#else
  msgBufSizeDefault;
#endif
  
private:
  // uphold that this is null terminated
  static std::array<char, msgBufSize> m_msgBuf;

  static constexpr void copyCstrToBuf_(const char* str) PF_NOEXCEPT {
    for (std::size_t i = 0; i < msgBufSize - 1; i++) {
      m_msgBuf[i] = str[i];
      if (m_msgBuf[i] == '\0') {
        return;
      }
    }
    m_msgBuf.back() = '\0';
  }

  static constexpr void copyStrToBuf_(const std::string_view& str) PF_NOEXCEPT {
    std::size_t i = 0;
    while(true) {
      if (i == str.size() || i == m_msgBuf.size() - 1)
        m_msgBuf[i] = '\0';
        
      m_msgBuf[i] = str[i];
      i++;
    }
  }

public:
  Exception(void) PF_NOEXCEPT = delete;
  constexpr Exception(const Exception&) PF_NOEXCEPT = default;
  constexpr explicit Exception(const std::string_view& str) PF_NOEXCEPT { copyStrToBuf_(str); }
  constexpr explicit Exception(const char * str) PF_NOEXCEPT { copyCstrToBuf_(str); }
  constexpr explicit Exception(const std::exception& error) PF_NOEXCEPT { copyCstrToBuf_(error.what()); }

  constexpr const char* what(void) const PF_NOEXCEPT { return m_msgBuf.data(); }
};

std::array<char, Exception::msgBufSize> Exception::m_msgBuf = {};

}
