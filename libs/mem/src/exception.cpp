module;

#include <PixelForge/core/macros.hpp>

module PixelForge.mem.exception;

namespace pf {

namespace mem {

AlignmentError::AlignmentError(std::size_t requestedAlignment, std::size_t minAlignment) PF_NOEXCEPT
  : Exception(what_arg),
    m_requestedAlignment(requestedAlignment),
    m_minAlignment(minAlignment) {
}

std::size_t
AlignmentError::requestedAlignment() const PF_NOEXCEPT {
  return m_minAlignment;
}

std::size_t
AlignmentError::minAlignmnet() const PF_NOEXCEPT {
  return m_minAlignment;
}

OOMError::OOMError(std::size_t requested, std::size_t needed, std::size_t available) PF_NOEXCEPT
  : Exception(what_arg),
    m_requested(requested),
    m_needed(needed),
    m_available(available) {
}

[[nodiscard]]
std::size_t
OOMError::requested() const PF_NOEXCEPT {
  return m_requested;
}

[[nodiscard]]
std::size_t
OOMError::needed() const PF_NOEXCEPT {
  return m_needed;
}

[[nodiscard]]
std::size_t
OOMError::available() const PF_NOEXCEPT {
  return m_available;
}

}

}
