module;

#include <type_traits>

#include <PixelForge/core/macro.hpp>

export module PixelForge.exception;

namespace pf {

namespace priv {

class ExceptionImpl {
public:
  ExpceptionImpl(void) PF_NOEXCEPT;
};

}

export
class Exception : private ExceptionImpl {
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
