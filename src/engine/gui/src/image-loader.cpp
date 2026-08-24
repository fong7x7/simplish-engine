#include "engine/gui/image-loader.h"

#include <engine/core/logger.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#include <stb_image.h>
#pragma clang diagnostic pop

#include <string>

namespace eng {
namespace {

  /// Number of channels to request from stb_image (always RGBA).
  constexpr size_t DESIRED_CHANNELS = 4;

  /// Build an ImageData from raw stb_image output and free the raw buffer.
  ImageData buildImageData(stbi_uc* raw, int w, int h, int channels) {
    const auto byte_count =
        static_cast<size_t>(w) * static_cast<size_t>(h) * DESIRED_CHANNELS;

    ImageData result;
    result.width = static_cast<uint32_t>(w);
    result.height = static_cast<uint32_t>(h);
    result.source_channels = static_cast<uint32_t>(channels);
    result.pixels.assign(raw, raw + byte_count);

    stbi_image_free(raw);
    return result;
  }

}  // namespace

std::optional<ImageData> ImageLoader::loadFromFile(std::string_view path) {
  const std::string path_str(path);

  int w = 0;
  int h = 0;
  int channels = 0;
  auto* raw = stbi_load(path_str.c_str(), &w, &h, &channels,
                        static_cast<int>(DESIRED_CHANNELS));
  if (raw == nullptr) {
    Logger::warn("ImageLoader",
                 std::string("failed to load '") + path_str + "'");
    return std::nullopt;
  }
  return buildImageData(raw, w, h, channels);
}

}  // namespace eng
