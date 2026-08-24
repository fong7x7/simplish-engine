#pragma once

#include "ps5-platform-config.h"
#include "ps5-types.h"

#include <cstdint>
#include <engine/core/event-bus.h>

namespace eng {

/// Configuration for the GNM RHI backend.
struct Ps5GnmConfig {
  /// GPU memory budget in megabytes for GNM allocations.
  uint32_t gpu_memory_budget_mb = PS5_GPU_MEMORY_BUDGET_DEFAULT_MB;
  /// Initial render mode (Performance or Quality).
  Ps5RenderMode render_mode = Ps5RenderMode::PERFORMANCE;
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
};

}  // namespace eng
