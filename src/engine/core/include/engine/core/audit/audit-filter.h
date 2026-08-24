#pragma once

#include "audit-types.h"

#include <cstdint>
#include <optional>
#include <string>

namespace eng {

struct AuditFilter {
  /// Filter by event category ID (nullopt matches all).
  std::optional<uint16_t> category{};
  /// Filter by event type ID within category (nullopt matches all).
  std::optional<uint16_t> event_type{};
  /// Filter by actor entity ID (nullopt matches all).
  std::optional<int64_t> actor_id{};
  /// Filter by target entity ID (nullopt matches all).
  std::optional<int64_t> target_id{};
  /// Minimum severity level (nullopt matches all).
  std::optional<AuditSeverity> min_severity{};
  /// Start of time range in nanoseconds (nullopt means no lower bound).
  std::optional<uint64_t> start_timestamp_ns{};
  /// End of time range in nanoseconds (nullopt means no upper bound).
  std::optional<uint64_t> end_timestamp_ns{};
  /// Regex pattern matched against event name and payload summary.
  std::optional<std::string> text_pattern{};
  /// Filter by error code (nullopt matches all).
  std::optional<uint32_t> error_code{};
  /// Required flags bitmask (bitwise AND; nullopt matches all).
  std::optional<AuditFlags> flags_mask{};
  /// Maximum number of results to return.
  uint32_t limit = 1000;
};

}  // namespace eng
