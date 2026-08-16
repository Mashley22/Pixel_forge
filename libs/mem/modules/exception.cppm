module;

#include <string_view>

#include <PixelForge/core/macros.hpp>

export module PixelForge.mem.exception;

import PixelForge.core;

namespace pf {

export namespace mem {

class AlignmentError : public Exception {
public:
  AlignmentError(std::size_t requestedAlignment, std::size_t minAlignment) PF_NOEXCEPT;

  static constexpr std::string_view what_arg = "Mem alignment";

  [[nodiscard]]
  std::size_t
  requestedAlignment() const PF_NOEXCEPT;

  [[nodiscard]]
  std::size_t
  minAlignmnet() const PF_NOEXCEPT;

private:
  const std::size_t m_requestedAlignment;
  const std::size_t m_minAlignment;
};

// available should be adjusted for the given alignment!!
class OOMError : public Exception {
public:

  static constexpr std::string_view what_arg = "OOM";
  
  OOMError(std::size_t requested, 
           std::size_t needed,
           std::size_t available) PF_NOEXCEPT;
  
  [[nodiscard]]
  std::size_t
  requested() const PF_NOEXCEPT;

  [[nodiscard]]
  std::size_t 
  needed() const PF_NOEXCEPT;

  [[nodiscard]]
  std::size_t 
  available() const PF_NOEXCEPT;

private:
  const std::size_t m_requested;
  const std::size_t m_needed;
  const std::size_t m_available;
};

}

}
