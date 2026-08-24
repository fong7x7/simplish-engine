#pragma once

#include <cstdint>

namespace eng {

// Constants used by XboxDx12Config
constexpr uint32_t XBOX_GPU_MEMORY_BUDGET_DEFAULT_MB = 5120;
constexpr uint32_t XBOX_BACK_BUFFER_COUNT = 2;
constexpr uint32_t XBOX_DEFAULT_RENDER_WIDTH = 2560;
constexpr uint32_t XBOX_DEFAULT_RENDER_HEIGHT = 1440;

struct XboxDx12Config {
  /// GPU memory budget in megabytes for DX12 allocations.
  uint32_t gpu_memory_budget_mb = XBOX_GPU_MEMORY_BUDGET_DEFAULT_MB;
  /// Number of swap chain back buffers.
  uint32_t back_buffer_count = XBOX_BACK_BUFFER_COUNT;
  /// Render target width in pixels.
  uint32_t render_width = XBOX_DEFAULT_RENDER_WIDTH;
  /// Render target height in pixels.
  uint32_t render_height = XBOX_DEFAULT_RENDER_HEIGHT;
};

}  // namespace eng
