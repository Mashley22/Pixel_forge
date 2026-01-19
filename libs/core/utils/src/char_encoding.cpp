module;

#include <string>

module PixelForge.core.utils.charEncoding;

namespace pf {

InvalidCharToHexError::InvalidCharToHexError(char chr) : Exception(std::string(1, chr)) {};

InvalidHexToCharError::InvalidHexToCharError(int val) : Exception(std::to_string(val)) {};

}
