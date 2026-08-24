#pragma once

#include <cstdint>
#include <vector>

namespace eng {

struct RhiCaptureResult {
  /// Encoded image data (PNG or JPEG bytes).
  std::vector<uint8_t> data{};
  /// Width of the captured image in pixels.
  uint32_t width = 0;
  /// Height of the captured image in pixels.
  uint32_t height = 0;
};

}  // namespace eng
