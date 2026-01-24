module;

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.mem.exception;

import PixelForge.core.exception;

namespace pf {

export namespace mem {

class AlignmentError : Exception {
public:
  AlignmentError(std::size_t requestedAlignment, std::size_t minAlignment) PF_NOEXCEPT;

  [[nodiscard]]
  std::size_t
  requestedAlignment(void) const PF_NOEXCEPT;

  [[nodiscard]]
  std::size_t
  minAlignmnet(void) const PF_NOEXCEPT;

private:
  const std::size_t m_requestedAlignment;
  const std::size_t m_minAlignment;
};

// available should be adjusted for the given alignment!!
class OOMError : Exception {
public:

  OOMError(std::size_t requested, 
           std::size_t needed,
           std::size_t available) PF_NOEXCEPT;
  
  [[nodiscard]]
  std::size_t
  requested(void) const PF_NOEXCEPT;

  [[nodiscard]]
  std::size_t 
  needed(void) const PF_NOEXCEPT;

  [[nodiscard]]
  std::size_t 
  available(void) const PF_NOEXCEPT;

private:
  const std::size_t m_requested;
  const std::size_t m_needed;
  const std::size_t m_available;
};

}

}
