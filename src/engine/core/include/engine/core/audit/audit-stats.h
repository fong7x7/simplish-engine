#pragma once

#include "audit-category-count.h"

#include <cstdint>
#include <vector>

namespace eng {

struct AuditStats {
  /// Total number of events in the merge buffer.
  uint64_t total_event_count{};
  /// Current event emission rate.
  float events_per_second{};
  /// Ring buffer fullness ratio (0.0 to 1.0).
  float ring_buffer_usage{};
  /// Total bytes written to disk across all .vxaudit files.
  uint64_t disk_usage_bytes{};
  /// Number of events dropped due to full ring buffers.
  uint64_t overflow_count{};
  /// Per-category event counts.
  std::vector<AuditCategoryCount> per_category;
};

}  // namespace eng
