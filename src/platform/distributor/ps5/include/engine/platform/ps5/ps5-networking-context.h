#pragma once

#include "ps5-types.h"

#include <engine/core/event-bus.h>

namespace eng {

/// Context for PSN networking state.
struct Ps5NetworkingContext {
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
  /// Handle to the currently active PSN game session.
  Ps5SessionHandle active_session = PS5_SESSION_INVALID;
  /// Current PSN network connectivity status.
  Ps5NetworkStatus network_status = Ps5NetworkStatus::OFFLINE;
  /// PS5 user ID of the signed-in user.
  Ps5UserId signed_in_user = PS5_USER_ID_INVALID;
};

}  // namespace eng
