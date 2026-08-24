#pragma once

#include "rhi-core-types.h"

#include <cstdint>

namespace eng {

struct RhiCaptureRequest {
  /// Texture to capture (invalid/0 means the backbuffer).
  RhiTextureHandle target = RHI_TEXTURE_INVALID;
  /// Output image encoding format (PNG or JPEG).
  RhiCaptureFormat format = RhiCaptureFormat::PNG;
  /// Horizontal pixel offset of the capture region.
  uint32_t x = 0;
  /// Vertical pixel offset of the capture region.
  uint32_t y = 0;
  /// Width of the capture region (0 = full target width).
  uint32_t width = 0;
  /// Height of the capture region (0 = full target height).
  uint32_t height = 0;
  /// JPEG quality percentage (ignored for PNG).
  uint8_t jpeg_quality = 90;
};

}  // namespace eng
