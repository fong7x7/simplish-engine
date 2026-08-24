#pragma once

#include "ps5-types.h"

#include <cstdint>
#include <engine/core/event-bus.h>

namespace eng {

/// Configuration for PS5 platform initialisation.
struct Ps5PlatformConfig {
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
  /// GPU memory budget in megabytes for the PS5 graphics subsystem.
  uint32_t gpu_memory_budget_mb = PS5_GPU_MEMORY_BUDGET_DEFAULT_MB;
};

}  // namespace eng
