#pragma once

#include "rhi-types.h"

#include <cstdint>

namespace eng {

struct RhiRenderPassBeginInfo {
  /// Array of colour render target texture handles.
  const RhiTextureHandle* color_targets = nullptr;
  /// Number of colour render targets in the array.
  uint32_t color_target_count = 0;
  /// Depth/stencil render target texture handle.
  RhiTextureHandle depth_target = RHI_TEXTURE_INVALID;
  /// RGBA clear colour applied when colour load op is CLEAR.
  float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  /// Depth clear value applied when depth load op is CLEAR.
  float clear_depth = 1.0f;
  /// Stencil clear value applied when depth load op is CLEAR.
  uint8_t clear_stencil = 0;
  /// Load operation for colour attachments at pass begin.
  RhiLoadOp color_load_op = RhiLoadOp::CLEAR;
  /// Load operation for the depth attachment at pass begin.
  RhiLoadOp depth_load_op = RhiLoadOp::CLEAR;
  /// Target array layer for depth texture arrays (0 = first layer).
  uint32_t depth_array_layer = 0;
};

}  // namespace eng
