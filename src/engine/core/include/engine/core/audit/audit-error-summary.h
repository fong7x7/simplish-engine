#pragma once

#include <cstdint>
#include <string>

namespace eng {

struct AuditErrorSummary {
  /// Numeric error code.
  uint32_t error_code{};
  /// Symbolic name of the error.
  std::string symbol;
  /// Total number of occurrences of this error.
  uint64_t occurrence_count{};
  /// Timestamp of the first occurrence in nanoseconds.
  uint64_t first_timestamp_ns{};
  /// Timestamp of the most recent occurrence in nanoseconds.
  uint64_t last_timestamp_ns{};
};

}  // namespace eng
