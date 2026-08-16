module;

#include <cstddef>
#include <vector>
#include <unordered_map>

export module PixelForge.validation_helpers;

namespace pf_vh {

export class LifeTimeTracker {
public:
  enum class OpType {
    DEFAULT_CONSTRUCT,
    CONSTRUCT,
    COPY_CONSTRUCT,
    COPY_ASSIGN,
    MOVE_CONSTRUCT,
    MOVE_ASSIGN,
    DESTRUCT
  };

  struct OpInfo {
    std::size_t id;
    OpType type;

    bool operator==(const OpInfo&) const = default;
  };

  static const std::unordered_map<const LifeTimeTracker*, std::vector<OpInfo>>&
  opLogs() { return s_opLogs; }

  static void
  clearOpLogs() { s_opLogs.clear(); }

  struct DeferClear {
    ~DeferClear() noexcept {
      s_opLogs.clear();
    }
  };

  LifeTimeTracker() : m_id(s_counter++) {
    log_(OpType::DEFAULT_CONSTRUCT);
  }

  LifeTimeTracker(int /*unused*/) : m_id(s_counter++) {
    log_(OpType::CONSTRUCT);
  } 

  LifeTimeTracker(LifeTimeTracker&& /*unused*/) : m_id(s_counter++) {
    log_(OpType::MOVE_CONSTRUCT);
  }

  LifeTimeTracker(LifeTimeTracker& /*unused*/) : m_id(s_counter++) {
    log_(OpType::COPY_CONSTRUCT);
  }

  LifeTimeTracker& operator=(LifeTimeTracker& other) {
    log_(OpType::COPY_ASSIGN);
    std::swap(m_id, other.m_id);
    return *this;
  }

  LifeTimeTracker& operator=(LifeTimeTracker&& other) {
    log_(OpType::COPY_ASSIGN);
    std::swap(m_id, other.m_id);
    return *this;
  }

  ~LifeTimeTracker() {
    log_(OpType::DESTRUCT);
  }

private:
  void log_(const OpType&& type) {
    s_opLogs[this].push_back({ 
      .id = m_id,
      .type = type
    });
  }

  std::size_t m_id;
  static std::unordered_map<
    const LifeTimeTracker*,
    std::vector<OpInfo>
  > s_opLogs;
  static std::size_t s_counter;
};

export struct alignas(LifeTimeTracker) LifeTimeTrackerStorage {
  unsigned char data[sizeof(LifeTimeTracker)];
};

std::unordered_map<
  const LifeTimeTracker*,
  std::vector<LifeTimeTracker::OpInfo>
> LifeTimeTracker::s_opLogs;

std::size_t LifeTimeTracker::s_counter{0};

}
