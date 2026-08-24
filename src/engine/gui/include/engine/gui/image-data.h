#pragma once

#include <cstdint>
#include <vector>

namespace eng {

/// Decoded image pixel data in RGBA format (row-major, tightly packed).
/// Produced by ImageLoader; consumed by RHI texture creation.
/// @thread_safety Main thread only.
struct ImageData {
  /// Image width in pixels.
  uint32_t width = 0;
  /// Image height in pixels.
  uint32_t height = 0;
  /// Number of channels in the original file (informational; pixels are
  /// always converted to 4-channel RGBA).
  uint32_t source_channels = 0;
  /// RGBA pixel data, row-major, tightly packed (width * height * 4 bytes).
  std::vector<uint8_t> pixels{};
};

}  // namespace eng
