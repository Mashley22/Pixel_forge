module;

#include <stdexcept>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.exception;

import PixelForge.core.require;

namespace pf {

export
class Exception : public std::runtime_error {
public:
  Exception(void) PF_NOEXCEPT = delete;
  constexpr Exception(const Exception&) = default;
  constexpr Exception(const std::string& str) : std::runtime_error(str) {}
  constexpr Exception(const std::string_view& str) : std::runtime_error(std::string(str)) {}
  constexpr Exception(const char * str) : std::runtime_error(str) {}
  constexpr Exception(const std::runtime_error& error) : std::runtime_error(error) {}
};

}
