#pragma once

#include "image-data.h"

#include <optional>
#include <string_view>

namespace eng {

/// Loads raster images (PNG, JPG, BMP, TGA, GIF) from disk into RGBA pixel
/// data using stb_image. All functions are static; no instance state.
/// @thread_safety Main thread only.
struct ImageLoader {
  /// Load an image file and decode it to RGBA pixels.
  /// Returns std::nullopt if the file cannot be read or decoded.
  /// Supported formats: PNG, JPEG, BMP, TGA, GIF.
  static std::optional<ImageData> loadFromFile(std::string_view path);
};

}  // namespace eng
