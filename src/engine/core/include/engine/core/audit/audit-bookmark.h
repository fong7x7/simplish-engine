#pragma once

#include <cstdint>
#include <string>

namespace eng {

struct AuditBookmark {
  /// Human-readable bookmark name (unique within session).
  std::string name;
  /// Event timeline position in nanoseconds.
  uint64_t timestamp_ns{};
};

}  // namespace eng
