#pragma once

#include "audit-types.h"

#include <cstdint>

namespace eng {

struct AuditEmitParams {
  /// Event category ID.
  uint16_t category{};
  /// Event type ID within category.
  uint16_t event_type{};
  /// Event severity level.
  AuditSeverity severity{};
  /// Entity/user that caused the event (-1 for system).
  int64_t actor_id{};
  /// Target entity (-1 for none).
  int64_t target_id{};
  /// Event flags bitmask.
  AuditFlags flags{};
};

}  // namespace eng
