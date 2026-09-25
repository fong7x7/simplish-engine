#pragma once

/// @file water-scene-copy.h
/// @brief What the water copies the scene out of, to see through it.
/// @par Threading A value type.

#include <cstdint>
#include <engine/render/rhi-core-types.h>
#include <engine/render/rhi-types.h>

namespace eng {

/// The colour target the scene pass drew into, which the water copies once
/// that pass has ended: the backbuffer in the editor, an offscreen target
/// in a test.
struct WaterSceneCopy {
  /// The target the scene was drawn into.
  RhiTextureHandle source = RHI_TEXTURE_INVALID;
  /// Its width in pixels.
  uint32_t width = 0;
  /// Its height in pixels.
  uint32_t height = 0;
  /// Its format, which the copy is made in; `UNDEFINED` takes sRGB RGBA8.
  RhiFormat format = RhiFormat::UNDEFINED;
};

}  // namespace eng
