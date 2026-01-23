module;

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.mem.exception;

import PixelForge.core.exception;

namespace pf {

namespace mem {

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
  struct AlignmentInfo {
    std::size_t requested;
    std::size_t min;
    std::size_t given;
  };
  
  struct SizeInfo {
    std::size_t requested;
    std::size_t available;
  };

  OOMError(std::size_t requestedAlignment, 
           std::size_t minAlignment,
           std::size_t givenAlignment,
           std::size_t requestedSize,
           std::size_t availableSize) PF_NOEXCEPT;

  OOMError(const AlignmentInfo& alignInfo, const SizeInfo& sizeInfo) PF_NOEXCEPT;

  const SizeInfo& size(void) const PF_NOEXCEPT;

  const AlignmentInfo& alignment(void) const PF_NOEXCEPT;
  
private:
  const AlignmentInfo m_align;
  const SizeInfo m_size;
};

}

}
