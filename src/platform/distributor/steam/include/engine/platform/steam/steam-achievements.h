#pragma once

// Design Summary -- Steam Achievements & Stats
// Technical Approach:
// docs/technical-approaches/engine/platform-steam/achievements-stats.md
//
// Behaviours:
//   - Unlock a Steam achievement by string ID (SetAchievement + StoreStats)
//   - Set an integer stat by name
//   - Set a float stat by name
//   - Flush dirty stats to Steam servers (periodic 60s timer + shutdown)
//   - Tick the periodic flush timer each frame
//
// Edge Cases:
//   - Steam not available: all operations no-op at debug log level
//   - Achievement already unlocked: idempotent, no error
//   - Invalid achievement/stat name: Steamworks returns false; log warning
//   - StoreStats() network failure: log warning; retry on next flush
//   - Tick called after shutdown: no-op
//
// Invariants:
//   - StoreStats() called at most once per 60 seconds + once at shutdown
//   - Achievement unlock is immediate; stat updates are batched
//   - Main thread only
//
// Integration Points:
//   - Game achievement system: system_unlock_achievement() dispatches here on
//   Steam
//   - Audit system: achievement unlock can emit audit entries

#include "steam-types.h"

#include <cstdint>
#include <string_view>

namespace eng {

// Forward declaration
struct SteamContext;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Unlock a Steam achievement. Calls SetAchievement() + StoreStats()
/// immediately. No-op if Steam is unavailable or achievement already unlocked.
/// Main thread only.
void unlockAchievement(const SteamContext& ctx,
                       std::string_view achievement_id);

/// Set an integer stat. The stat is batched and flushed on the next
/// periodic StoreStats() call or at shutdown.
/// Main thread only.
void setStatInt(const SteamContext& ctx, std::string_view stat_name,
                int32_t value);

/// Set a float stat. The stat is batched and flushed on the next
/// periodic StoreStats() call or at shutdown.
/// Main thread only.
void setStatFloat(const SteamContext& ctx, std::string_view stat_name,
                  float value);

/// Flush all dirty stats to Steam servers immediately. Called by
/// shutdownSteam() and by tickStats() when the flush interval elapses.
/// Main thread only.
void flushStats(const SteamContext& ctx);

/// Advance the periodic stats flush timer. Call once per frame.
/// Flushes stats to Steam servers when STEAM_STATS_FLUSH_INTERVAL_S
/// elapses since the last flush. No-op if no stats are dirty.
/// Main thread only.
void tickStats(const SteamContext& ctx, float dt);

}  // namespace eng
