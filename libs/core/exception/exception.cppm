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
  Exception(void);

  Exception(const std::string_view msg);

  Exception(std::string&& msg) PF_NOEXCEPT;

  [[nodiscard]]
  std::string_view
  what(void) PF_NOEXCEPT;

private:
  std::string m_msg;
};

}
