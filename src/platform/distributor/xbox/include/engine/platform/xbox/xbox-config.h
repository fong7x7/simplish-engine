#pragma once

#include <engine/core/event-bus.h>

namespace eng {

struct XboxConfig {
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
};

}  // namespace eng
