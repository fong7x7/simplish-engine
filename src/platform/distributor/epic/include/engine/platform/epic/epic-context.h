#pragma once

#include "epic-product-user-id.h"

#include <engine/core/event-bus.h>

namespace eng {

/// Root context for the Epic platform layer. Returned by initEpic().
/// Passed to all Epic subsystem functions.
struct EpicContext {
  /// Product user ID of the locally authenticated user.
  EpicProductUserId local_user_id = EPIC_USER_ID_INVALID;
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
};

}  // namespace eng
