module;

#include <chrono>
#include <cstdint>
#include <cstring>

#include <PixelForge/core/macros.hpp>

export module PixelForge.logging.backends.fast:types;

import PixelForge.core;
import PixelForge.containers;
import PixelForge.mem;
import PixelForge.logging;

namespace pf::log::fast_backend {

using clock = std::chrono::system_clock;
using size_type = std::size_t;

using time_point_t = clock::time_point;

static_assert(std::is_convertible_v<decltype(Record::time), time_point_t>);

export constexpr std::size_t block_size = PIXELFORGE_FAST_LOG_BACKEND_BLOCK_SIZE;

static_assert(sizeof(time_point_t) == sizeof(std::uint64_t));

PF_TEST_EXPORT
struct Header {
  time_point_t time{};
  std::uint32_t id{};
  std::uint16_t size{0};
  Level level{Level::DEBUG};
  bool is_last{true};
};

static_assert(sizeof(Header) == 2 * sizeof(time_point_t));
static_assert(alignof(Header) == alignof(time_point_t));

PF_TEST_EXPORT
struct Payload {
  Header header;

  [[nodiscard]] constexpr std::span<const char>
  data() const PF_NOEXCEPT {
    return std::span<const char>(pointer_cast<const char*>(this + 1), header.size);
  }

  [[nodiscard]] constexpr std::span<char>
  data() PF_NOEXCEPT {
    return std::span<char>(pointer_cast<char*>(this + 1), header.size);
  }

  // points one beyond the end of the payload
  [[nodiscard]] constexpr char*
  end() const PF_NOEXCEPT {
    return pointer_cast<char*>(pointer_cast<std::uintptr_t>(this + 1) + header.size);
  }

  [[nodiscard]] constexpr Payload&
  next() const PF_NOEXCEPT {
    PF_REQUIRE(!header.is_last);
    return *std::launder(pointer_cast<Payload*>(end()));
  }
};

PF_TEST_EXPORT
struct Block {
public:
  PF_PURE_FUNC
  Block*
  next() const PF_NOEXCEPT {
    return m_next;
  }

  // returns the number of payload bytes recorded
  PF_PURE_FUNC
  [[nodiscard]] constexpr size_type
  add_record(std::uint32_t id, const Record& record) {
    constexpr size_type iota = 64;
    const size_type remaining = max_remaining_payload_capacity();

    if (remaining < record.msg.size() && remaining < iota) {
      return 0;
    }

    last_payload()->header.is_last = false;

    Payload* new_payload =
        std::construct_at(pointer_cast<Payload*>(last_payload()->end()));
    new_payload->header = Header{
        .time = record.time,
        .id = id,
        .size = static_cast<std::uint16_t>(std::max(iota, record.msg.size())),
        .level = record.level,
        .is_last = true,
    };

    std::memcpy(new_payload->data().data(), record.msg.data(), new_payload->header.size);

    return new_payload->header.size;
  }

private:
  alignas(alignof(Payload)) Block* m_next{nullptr};

  Payload m_first{.header = {.is_last = true}};

  PF_PURE_FUNC [[nodiscard]]
  constexpr const Payload*
  last_payload() const PF_NOEXCEPT {
    const Payload* next_payload = &m_first;
    while (!next_payload->header.is_last) {
      next_payload = &next_payload->next();
    }
    return next_payload;
  }

  PF_PURE_FUNC [[nodiscard]]
  constexpr Payload*
  last_payload() PF_NOEXCEPT {
    Payload* next_payload = &m_first;
    while (!next_payload->header.is_last) {
      next_payload = &next_payload->next();
    }
    return next_payload;
  }

  PF_PURE_FUNC [[nodiscard]]
  constexpr size_type
  max_remaining_payload_capacity() const PF_NOEXCEPT {
    const auto start = pointer_cast<std::uintptr_t>(this);

    const std::size_t used_capacity =
        pointer_cast<std::uintptr_t>(last_payload()->end()) - start;

    return block_size - used_capacity;
  }
};

}
