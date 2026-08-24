#pragma once

// Design Summary -- Epic Achievements, Stats & Leaderboards
// Technical Approach:
//   docs/technical-approaches/engine/platform-epic/achievements-stats.md
//
// Behaviours:
//   - Unlock achievements via EOS_Achievements_UnlockAchievements()
//   - Ingest stat updates (batched, flushed every 60 s and on shutdown)
//   - Query global and friends leaderboard rankings
//
// Edge Cases:
//   - EOS not available: operations no-op (logged)
//   - Achievement already unlocked: idempotent success
//   - Stat ingest with zero amount: skipped
//   - Leaderboard query with no results: empty vector in callback
//
// Invariants:
//   - Achievement/stat definitions are in Epic Developer Portal
//   - Stats are batched in-memory; individual calls do not hit network
//   - All operations main-thread-only
//
// Integration Points:
//   - Game achievement system: dispatches to epicUnlockAchievement()
//   - Game stats: epicIngestStat() for platform persistence

#include "epic-types.h"

#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <functional>
#include <string_view>
#include <vector>

namespace eng {

// Forward declarations
struct EpicContext;

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------

using EpicLeaderboardCallback = std::function<void(
    std::expected<std::vector<EpicLeaderboardEntry>, EpicError> result)>;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Unlock an achievement by ID. Fire-and-forget; failure logged.
/// Idempotent -- unlocking an already-unlocked achievement succeeds.
/// Main thread only.
void epicUnlockAchievement(const EpicContext& ctx,
                           std::string_view achievement_id);

/// Ingest a stat update. Batched in-memory; not sent to EOS
/// immediately. Call epicFlushStats() to send the batch.
/// Zero amount is ignored.
/// Main thread only.
void epicIngestStat(const EpicContext& ctx, std::string_view stat_name,
                    int32_t amount);

/// Flush all batched stat updates to EOS. Called automatically
/// every 60 s and on shutdown. Safe to call when batch is empty.
/// Main thread only.
void epicFlushStats(const EpicContext& ctx);

/// Query global leaderboard rankings. Async; callback receives
/// entries sorted by rank or error.
/// Main thread only.
void epicQueryLeaderboardRanks(const EpicContext& ctx,
                               std::string_view leaderboard_id,
                               uint32_t max_results,
                               const EpicLeaderboardCallback& callback);

/// Query friends leaderboard rankings. Async; callback receives
/// entries for friends only, sorted by rank.
/// Main thread only.
void epicQueryLeaderboardFriends(const EpicContext& ctx,
                                 std::string_view leaderboard_id,
                                 const EpicLeaderboardCallback& callback);

}  // namespace eng
