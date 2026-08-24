#pragma once

// Design Summary -- Epic Social (Friends & Presence)
// Technical Approach:
//   docs/technical-approaches/engine/platform-epic/social.md
//
// Behaviours:
//   - Query friends list (async, populates local cache)
//   - Get friend count, friend-by-index, friend status from cache
//   - Set rich presence (status, rich text, join info)
//
// Edge Cases:
//   - EOS not available: functions return empty/error
//   - Friends query fails: cached list remains stale
//   - Friend index out of range: returns invalid PUID
//   - Set presence when not authenticated: no-op
//
// Invariants:
//   - Friends list cached locally after query
//   - All social functions main-thread-only
//   - Presence updates are fire-and-forget
//
// Integration Points:
//   - Matchmaking: friends list for invite targets
//   - Game UI: friends/presence display
//   - EventBus: presence change events emitted

#include "epic-types.h"

#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <functional>
#include <string>
#include <string_view>

namespace eng {

// Forward declarations
struct EpicContext;

// ---------------------------------------------------------------------------
// Presence info
// ---------------------------------------------------------------------------

struct EpicPresenceInfo {
  /// Current online status of the user.
  EpicPresenceStatus status = EpicPresenceStatus::ONLINE;
  /// Descriptive text shown to friends (e.g. "Exploring Crimson Valley").
  std::string rich_text{};
  /// Connection string that friends use to join the game session.
  std::string join_info{};
};

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------

using EpicFriendsQueryCallback =
    std::function<void(std::expected<uint32_t, EpicError> friend_count)>;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Query the friends list from EOS. Async; callback receives the
/// friend count or error. Populates local cache on success.
/// Main thread only.
void epicQueryFriends(const EpicContext& ctx,
                      const EpicFriendsQueryCallback& callback);

/// Returns the cached friend count. Returns 0 if cache is empty
/// or EOS is unavailable. Main thread only.
uint32_t epicGetFriendCount(const EpicContext& ctx);

/// Returns the PUID of the friend at the given index in the cached
/// list. Returns EPIC_USER_ID_INVALID if index is out of range.
/// Main thread only.
EpicProductUserId epicGetFriendAtIndex(const EpicContext& ctx, uint32_t index);

/// Returns the friendship status with the given user.
/// Returns NOT_FRIENDS if EOS is unavailable or user not found.
/// Main thread only.
EpicFriendStatus epicGetFriendStatus(const EpicContext& ctx,
                                     EpicProductUserId user);

/// Set the local user's rich presence. Fire-and-forget.
/// No-op if EOS is unavailable or not authenticated.
/// Main thread only.
void epicSetPresence(const EpicContext& ctx, const EpicPresenceInfo& presence);

}  // namespace eng
