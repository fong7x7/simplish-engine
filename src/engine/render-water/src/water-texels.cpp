#include <algorithm>
#include <engine/render-water/water-texels.h>
#include <utility>

namespace eng {

namespace {

  /// @p value, from −1 to 1, as a unorm byte about the middle. Rounded by
  /// adding a half and truncating, which the clamp keeps positive.
  uint8_t signedUnorm(float value) {
    return static_cast<uint8_t>(128.0f +
                                127.5f * std::clamp(value, -1.0f, 1.0f));
  }

  /// @p value, from 0 to 1, as a unorm byte.
  uint8_t unorm(float value) {
    return static_cast<uint8_t>(0.5f + 255.0f * std::clamp(value, 0.0f, 1.0f));
  }

  /// @p field's level at (@p x, @p y), held at the edge.
  float heldLevel(const WaterField& field, int64_t x, int64_t y) {
    const auto cx = std::clamp<int64_t>(x, 0, field.width - 1);
    const auto cy = std::clamp<int64_t>(y, 0, field.height - 1);
    return field
        .level[static_cast<size_t>(cy) * field.width + static_cast<size_t>(cx)];
  }

  /// One texel for sample (@p x, @p y), written at @p out.
  void writeTexel(const WaterField& field, int64_t x, int64_t y, uint8_t* out) {
    // Central differences over two samples, in tiles of rise per tile.
    const float per_tile = 0.5f * static_cast<float>(field.samples_per_tile);
    const float slope_x =
        (heldLevel(field, x + 1, y) - heldLevel(field, x - 1, y)) * per_tile;
    const float slope_y =
        (heldLevel(field, x, y + 1) - heldLevel(field, x, y - 1)) * per_tile;
    const size_t i =
        static_cast<size_t>(y) * field.width + static_cast<size_t>(x);
    out[0] = signedUnorm(slope_x / WATER_SLOPE_RANGE);
    out[1] = signedUnorm(slope_y / WATER_SLOPE_RANGE);
    out[2] = signedUnorm(field.level[i] / WATER_LEVEL_RANGE);
    out[3] = unorm(field.shore[i] / WATER_SHORE_TILES);
  }

}  // namespace

void writeWaterTexels(const WaterField& field, std::vector<uint8_t>& texels) {
  texels.resize(static_cast<size_t>(field.width) * field.height *
                WATER_TEXEL_BYTES);
  for (int64_t y = 0; std::cmp_less(y, field.height); ++y) {
    for (int64_t x = 0; std::cmp_less(x, field.width); ++x) {
      const size_t i =
          static_cast<size_t>(y) * field.width + static_cast<size_t>(x);
      writeTexel(field, x, y, texels.data() + i * WATER_TEXEL_BYTES);
    }
  }
}

}  // namespace eng
