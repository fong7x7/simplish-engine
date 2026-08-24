#pragma once

#include <cstdint>
#include <engine/core/event-bus.h>

namespace eng {

struct SteamConfig {
  /// Steam application ID for this game.
  uint32_t app_id = 0;
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
};

}  // namespace eng
