module;

#include <string_view>
#include <string>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.exception;

namespace pf {

namespace priv {

class ExceptionImpl {
public:
  ExceptionImpl(void) PF_NOEXCEPT;
};

}

export
class Exception : private priv::ExceptionImpl {
public:
  Exception(void) PF_NOEXCEPT;
};

}
