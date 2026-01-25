module;

#include <cstdint>
#include <string>

#include <PixelForge/core/macros.hpp>

export module PixelForge.log_decoder;

import PixelForge.logging.logCodes;

namespace pf::log {

[[nodiscard]]
std::size_t
codeDataLen(const Code& code) PF_NOEXCEPT;

[[nodiscard]]
std::string
codeStr(const Code& code, std::uint64_t * p_data) PF_NOEXCEPT;

}
