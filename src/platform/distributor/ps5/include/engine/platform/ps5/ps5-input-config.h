#pragma once

#include "ps5-types.h"

#include <engine/core/event-bus.h>

namespace eng {

/// Configuration for the PS5 input backend.
struct Ps5InputConfig {
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
  /// Whether controller vibration is enabled at startup.
  Ps5VibrationState vibration_state = Ps5VibrationState::ENABLED;
};

}  // namespace eng
