#pragma once

#include "ps5-user-id.h"

#include <engine/core/event-bus.h>

namespace eng {

/// Root context for the PS5 platform layer. Returned by initPs5Platform().
/// Passed to all PS5 subsystem functions.
struct Ps5PlatformContext {
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
  /// PS5 user ID of the locally signed-in user.
  Ps5UserId local_user_id = PS5_USER_ID_INVALID;
  /// True when the application is in suspended state.
  bool suspended = false;
};

}  // namespace eng
