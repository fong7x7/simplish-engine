#pragma once

#include "steam-user-id.h"

#include <cstdint>
#include <engine/core/event-bus.h>

namespace eng {

/// Root context for the Steam platform layer. Returned by initSteam().
/// Passed to all Steam subsystem functions.
struct SteamContext {
  /// Steam application ID for this game.
  uint32_t app_id = 0;
  /// Steam ID of the locally signed-in user.
  SteamUserId local_user_id = STEAM_USER_ID_INVALID;
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
};

}  // namespace eng
