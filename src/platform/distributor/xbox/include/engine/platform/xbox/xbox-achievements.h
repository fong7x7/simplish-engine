#pragma once

// Design Summary -- Xbox Series X Achievements
// Technical Approach:
// docs/technical-approaches/engine/platform-xbox-series-x/achievements.md
//
// Behaviours:
//   - Unlock a binary Xbox achievement (set progress to 100%)
//   - Set incremental achievement progress (0-100%)
//   - Fire-and-forget: calls do not block or return results
//   - Map engine's system_unlock_achievement() to Xbox progress update
//
// Edge Cases:
//   - Xbox Live unavailable: log warning, no-op
//   - Achievement already at 100%: XSAPI handles idempotently
//   - Invalid achievement ID: XSAPI returns error, log warning
//   - User not signed in: no-op with warning
//   - Network failure: XSAPI queues retry internally
//
// Invariants:
//   - Achievement unlocks are fire-and-forget
//   - Xbox achievements use progress model (0-100%)
//   - Binary achievements set progress to 100 on unlock
//   - Main thread only
//
// Integration Points:
//   - Engine achievement system: system_unlock_achievement() dispatches here
//   - Xbox Live user: requires signed-in user for XSAPI calls

#include "xbox-types.h"

#include <cstdint>
#include <string_view>

namespace eng {

// Forward declaration
struct XboxContext;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Unlock an Xbox achievement by setting its progress to 100%.
/// Fire-and-forget: errors are logged but not propagated.
/// No-op if Xbox Live is unavailable or user is not signed in.
/// Main thread only.
void unlockXboxAchievement(const XboxContext& ctx,
                           std::string_view achievement_id);

/// Set incremental achievement progress (0 to 100). Use for
/// progress-based achievements. Setting to 100 is equivalent
/// to unlockXboxAchievement(). Fire-and-forget.
/// Main thread only.
void setXboxAchievementProgress(const XboxContext& ctx,
                                std::string_view achievement_id,
                                uint32_t percent);

}  // namespace eng
