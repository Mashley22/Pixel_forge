module;

#include <cstddef>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core.mem.linearAllocator;

import PixelForge.core.mem.exception;
import PixelForge.core.mem.align;
import PixelForge.core.math;

namespace pf {

namespace mem {

namespace detail {

class LinearAllocatorView_impl {
public:
  constexpr
  LinearAllocatorView_impl(void* buffer, std::size_t size) PF_NOEXCEPT
    : m_capacity(size), m_start(static_cast<std::byte*>(buffer)), m_current(m_start) {}

  constexpr
  LinearAllocatorView_impl(std::byte* buffer, std::size_t size) PF_NOEXCEPT
    : m_capacity(size), m_start(buffer), m_current(m_start) {}

  [[nodiscard]] constexpr
  std::size_t
  cacpacity(void) const PF_NOEXCEPT { return m_capacity; }

  [[nodiscard]] constexpr 
  std::size_t
  remaining(void) const PF_NOEXCEPT { return m_capacity - used(); }

  [[nodiscard]] constexpr
  std::size_t
  used(void) const PF_NOEXCEPT { return static_cast<std::size_t>(m_current - m_start); }

  void constexpr 
  clear(void) PF_NOEXCEPT { m_current = 0; }
  
protected:
  
  /*
   *@throws pf::mem::AlignmentError if the alignments aren't powers of two or the alignment requested is smaller than the minAlignment
   *@throws pf::mem::OOMError if there isnt enough capacity to make the allocation 
  */
  [[nodiscard]] constexpr
  void*
  alloc(std::size_t size, std::size_t alignment, std::size_t minAlignment) {
    if (!math::isPowerOfTwo<std::size_t>(alignment) ||
        !math::isPowerOfTwo<std::size_t>(minAlignment)) {
      throw AlignmentError(alignment, minAlignment);
    }

    LinearAllocatorView_impl temp = *this;

    temp.m_current = align(m_current, alignment);
    
    if (temp.remaining() < size) {
      auto neededSize = [&]() {
        return size + static_cast<std::size_t>(temp.m_current - m_current);
      };
      throw OOMError(size, neededSize(), remaining());
    }

    m_current = temp.m_current + size;

    return temp.m_current;
  }

  /**
   *@throw pf::mem::OOMError if there isn't enough capacity
  */
  [[nodiscard]] constexpr
  void*
  alloc(std::size_t size) {
    if (remaining() < size) {
      throw OOMError(size, size, remaining());
    }
    
    std::byte * retVal = m_current;
    m_current+= size;

    return retVal;
  }

private:
  std::size_t m_capacity;
  std::byte * m_start;
  std::byte * m_current;
};

}

}

}
