module;

#include <string_view>

#include <PixelForge/core/macros.hpp>

export module PixelForge.mem:exception;

import PixelForge.core;

namespace pf {

export namespace mem {

class AlignmentError : public Exception {
public:
  static constexpr std::string_view what_arg =
      "{}: Mem alignment, requested alignment: {}, min alignmnet: {}";
  static constexpr std::size_t default_fmt_buf_size = 512;

  template <std::size_t FmtBufSize = default_fmt_buf_size>
  constexpr explicit AlignmentError(std::string_view caller,
                                    std::size_t requested_alignment,
                                    std::size_t min_alignment) PF_NOEXCEPT
    : Exception(fmt<FmtBufSize>(what_arg, caller, requested_alignment, min_alignment)),
      m_requested_alignment(requested_alignment),
      m_min_alignment(min_alignment) {}

  [[nodiscard]]
  constexpr std::size_t
  requested_alignment() const PF_NOEXCEPT {
    return m_requested_alignment;
  }

  [[nodiscard]]
  constexpr std::size_t
  min_alignmnet() const PF_NOEXCEPT {
    return m_min_alignment;
  }

private:
  const std::size_t m_requested_alignment;
  const std::size_t m_min_alignment;
};

// available should be adjusted for the given alignment!!
class OOMError : public Exception {
public:
  static constexpr std::string_view what_arg =
      "{}: OOM, requested: {}, needed: {}, available: {}";
  static constexpr std::size_t default_what_fmt_buf_size = 512;

  template <std::size_t FmtBufferSize = default_what_fmt_buf_size>
  OOMError(std::string_view caller,
           std::size_t requested,
           std::size_t needed,
           std::size_t available) PF_NOEXCEPT
    : Exception(fmt<FmtBufferSize>(what_arg, caller, requested, needed, available)
                    .to_str_view()),
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
