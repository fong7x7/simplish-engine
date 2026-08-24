#pragma once

#include "xbox-types.h"

#include <engine/core/event-bus.h>

namespace eng {

/// Xbox Live networking context. Tracks signed-in user, network state,
/// and event bus reference. GDK handles stored internally in .cpp.
struct XboxNetworkingContext {
  /// Xbox user ID of the locally signed-in user.
  XboxUserId local_user_id = XBOX_USER_ID_INVALID;
  /// Current Xbox Live network connectivity state.
  XboxNetworkState network_state = XboxNetworkState::UNKNOWN;
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
};

}  // namespace eng
