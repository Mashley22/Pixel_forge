module;

#include <PixelForge/core/macros.hpp>

module PixelForge.core.mem.exception;

namespace pf {

namespace mem {

AlignmentError::AlignmentError(std::size_t requestedAlignment, std::size_t minAlignment) PF_NOEXCEPT
  : m_requestedAlignment(requestedAlignment), m_minAlignment(minAlignment) {}

std::size_t
AlignmentError::requestedAlignment(void) const PF_NOEXCEPT { return m_minAlignment; }

std::size_t
AlignmentError::minAlignmnet(void) const PF_NOEXCEPT { return m_minAlignment; }

OOMError::OOMError(std::size_t requestedAlignment, 
                   std::size_t minAlignment,
                   std::size_t givenAlignment,
                   std::size_t requestedSize,
                   std::size_t availableSize) PF_NOEXCEPT
  : m_align({.requested = requestedAlignment,
             .min = minAlignment,
             .given = givenAlignment}), 
    m_size({.requested = requestedSize, 
            .available = availableSize}) {}

OOMError::OOMError(const AlignmentInfo& alignInfo, const SizeInfo& sizeInfo) PF_NOEXCEPT 
  : m_align(alignInfo), m_size(sizeInfo) {}

const OOMError::SizeInfo&
OOMError::size(void) const PF_NOEXCEPT {
  return m_size;
}

const OOMError::AlignmentInfo&
OOMError::alignment(void) const PF_NOEXCEPT {
  return m_align;
}

}

}
