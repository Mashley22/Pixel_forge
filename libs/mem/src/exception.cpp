module;

#include <PixelForge/core/macros.hpp>

module PixelForge.mem.exception;

namespace pf {

namespace mem {

AlignmentError::AlignmentError(std::size_t requestedAlignment, std::size_t minAlignment) PF_NOEXCEPT
  : m_requestedAlignment(requestedAlignment), m_minAlignment(minAlignment) {}

std::size_t
AlignmentError::requestedAlignment(void) const PF_NOEXCEPT { return m_minAlignment; }

std::size_t
AlignmentError::minAlignmnet(void) const PF_NOEXCEPT { return m_minAlignment; }

OOMError::OOMError(std::size_t requested, 
                   std::size_t needed,
                   std::size_t available) PF_NOEXCEPT
  : m_requested(requested), m_needed(needed), m_available(available) {}
  
[[nodiscard]]
std::size_t
OOMError::requested(void) const PF_NOEXCEPT {
  return m_requested;
}

[[nodiscard]]
std::size_t 
OOMError::needed(void) const PF_NOEXCEPT {
  return m_needed;
}

[[nodiscard]]
std::size_t 
OOMError::available(void) const PF_NOEXCEPT {
  return m_available;
}

}

}
