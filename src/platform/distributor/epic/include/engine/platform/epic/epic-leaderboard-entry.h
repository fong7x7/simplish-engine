#pragma once

#include "epic-product-user-id.h"

#include <cstdint>

namespace eng {

struct EpicLeaderboardEntry {
  /// Product user ID of the player who owns this entry.
  EpicProductUserId user_id;
  /// 1-based rank position on the leaderboard.
  uint32_t rank = 0;
  /// Score value for this leaderboard entry.
  int32_t score = 0;
};

}  // namespace eng
