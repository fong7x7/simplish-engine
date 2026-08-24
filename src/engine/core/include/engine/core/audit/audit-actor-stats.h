#pragma once

#include <cstdint>

namespace eng {

struct AuditActorStats {
  /// Actor entity ID.
  int64_t actor_id{};
  /// Number of events attributed to this actor.
  uint64_t event_count{};
};

}  // namespace eng
