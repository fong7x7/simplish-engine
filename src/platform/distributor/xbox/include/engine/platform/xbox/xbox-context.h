#pragma once

#include "xbox-types.h"

#include <engine/core/event-bus.h>

namespace eng {

/// Root context for the Xbox platform layer. Returned by initXbox().
/// Passed to all Xbox subsystem functions.
struct XboxContext {
  /// Xbox user ID of the locally signed-in user.
  XboxUserId local_user_id = XBOX_USER_ID_INVALID;
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
  /// Current GDK lifecycle state (running, suspending, etc.).
  XboxLifecycleState lifecycle_state = XboxLifecycleState::RUNNING;
};

}  // namespace eng
