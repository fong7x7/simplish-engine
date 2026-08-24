#pragma once

#include <engine/core/event-bus.h>

namespace eng {

/// Configuration for PS5 networking subsystem.
struct Ps5NetworkingConfig {
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
};

}  // namespace eng
