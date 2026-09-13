module;

#include <string_view>

#include <PixelForge/core/macros.hpp>

export module PixelForge.mem:exception;

import PixelForge.core;

namespace pf {

export namespace mem {

class AlignmentError : public Exception {
public:
  static constexpr std::string_view what_arg = "{}: Mem alignment, requested alignment: {}, min alignmnet: {}";
  static constexpr std::size_t defaultFmtBufSize = 512;

  template<std::size_t T_fmtBufSize = defaultFmtBufSize>
  constexpr explicit AlignmentError(std::string_view caller,
      std::size_t requestedAlignment,
                                    std::size_t minAlignment) PF_NOEXCEPT
    : Exception(fmt<T_fmtBufSize>(what_arg, caller, requestedAlignment, minAlignment)),
      m_requestedAlignment(requestedAlignment),
      m_minAlignment(minAlignment) {}

  [[nodiscard]]
  constexpr std::size_t
  requestedAlignment() const PF_NOEXCEPT {
    return m_requestedAlignment;
  }

  [[nodiscard]]
  constexpr std::size_t
  minAlignmnet() const PF_NOEXCEPT {
    return m_minAlignment;
  }

private:
  const std::size_t m_requestedAlignment;
  const std::size_t m_minAlignment;
};

// available should be adjusted for the given alignment!!
class OOMError : public Exception {
public:
  static constexpr std::string_view what_arg = "{}: OOM, requested: {}, needed: {}, available: {}";
  static constexpr std::size_t defaultWhatFmtBufSize = 512;

  template<std::size_t T_fmtBufferSize = defaultWhatFmtBufSize>
  OOMError(std::string_view caller, std::size_t requested, std::size_t needed, std::size_t available) PF_NOEXCEPT
    : Exception(fmt<T_fmtBufferSize>(what_arg, caller, requested, needed, available).toStrView()),
      m_requested(requested),
      m_needed(needed),
      m_available(available) {}

  [[nodiscard]]
  std::size_t
  requested() const PF_NOEXCEPT {
    return m_requested;
  }

  [[nodiscard]]
  std::size_t
  needed() const PF_NOEXCEPT {
    return m_needed;
  }

  [[nodiscard]]
  std::size_t
  available() const PF_NOEXCEPT {
    return m_available;
  }

private:
  const std::size_t m_requested;
  const std::size_t m_needed;
  const std::size_t m_available;
};

}

}
