module;

#include <string>

#include <PixelForge/core/macros.hpp>

module PixelForge.core.utils.charEncoding;

namespace pf {

InvalidCharToHexError::InvalidCharToHexError(char chr) PF_NOEXCEPT
  : m_chr(chr) {};

InvalidHexToCharError::InvalidHexToCharError(int val) PF_NOEXCEPT
  : m_val(val) {};

}
