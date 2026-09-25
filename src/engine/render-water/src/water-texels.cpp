#include <algorithm>
#include <cstring>
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

  /// Four bytes as the one little-endian word a texel is, so a texel is
  /// one store rather than four.
  uint32_t packed(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8U) |
           (static_cast<uint32_t>(b) << 16U) |
           (static_cast<uint32_t>(a) << 24U);
  }

  /// A row of the field and the rows either side of it, held at the edge.
  struct Rows {
    /// The row itself.
    const float* here = nullptr;
    /// The row to its south, or itself on the first row.
    const float* south = nullptr;
    /// The row to its north, or itself on the last row.
    const float* north = nullptr;
  };

  /// Row @p y of @p field's level, and its neighbours.
  Rows rowsAt(const WaterField& field, uint32_t y) {
    const float* here =
        field.level.data() + static_cast<size_t>(y) * field.width;
    return {here, y > 0 ? here - field.width : here,
            y + 1 < field.height ? here + field.width : here};
  }

  /// Row @p y of @p field's motion into @p out, a texel a sample: central
  /// differences over two samples, in tiles of rise per tile.
  void writeRow(const WaterField& field, uint32_t y, uint32_t* out) {
    const Rows rows = rowsAt(field, y);
    const float per_tile = 0.5f * static_cast<float>(field.samples_per_tile);
    const float* foam =
        field.foam.data() + static_cast<size_t>(y) * field.width;
    for (uint32_t x = 0; x < field.width; ++x) {
      const float west = rows.here[x > 0 ? x - 1 : x];
      const float east = rows.here[x + 1 < field.width ? x + 1 : x];
      out[x] =
          packed(signedUnorm((east - west) * per_tile / WATER_SLOPE_RANGE),
                 signedUnorm((rows.north[x] - rows.south[x]) * per_tile /
                             WATER_SLOPE_RANGE),
                 signedUnorm(rows.here[x] / WATER_LEVEL_RANGE), unorm(foam[x]));
    }
  }

  /// @p field's texels, one a sample, each written by @p write.
  template <typename Write>
  void writeEach(const WaterField& field, std::vector<uint8_t>& texels,
                 Write write) {
    texels.resize(static_cast<size_t>(field.width) * field.height *
                  WATER_TEXEL_BYTES);
    for (int64_t y = 0; std::cmp_less(y, field.height); ++y) {
      for (int64_t x = 0; std::cmp_less(x, field.width); ++x) {
        const size_t i =
            static_cast<size_t>(y) * field.width + static_cast<size_t>(x);
        write(x, y, texels.data() + i * WATER_TEXEL_BYTES);
      }
    }
  }

}  // namespace

void writeWaterTexels(const WaterField& field, std::vector<uint8_t>& texels) {
  texels.resize(static_cast<size_t>(field.width) * field.height *
                WATER_TEXEL_BYTES);
  // A row at a time into a word-aligned row of its own, copied out whole:
  // one store a texel, and none of them near the level being read.
  std::vector<uint32_t> row(field.width);
  for (uint32_t y = 0; y < field.height; ++y) {
    writeRow(field, y, row.data());
    std::memcpy(texels.data() +
                    static_cast<size_t>(y) * field.width * WATER_TEXEL_BYTES,
                row.data(), row.size() * sizeof(uint32_t));
  }
}

void writeWaterStillTexels(const WaterField& field,
                           std::vector<uint8_t>& texels) {
  writeEach(field, texels, [&](int64_t x, int64_t y, uint8_t* out) {
    const size_t i =
        static_cast<size_t>(y) * field.width + static_cast<size_t>(x);
    out[0] = unorm(field.shore[i] / WATER_SHORE_TILES);
    out[1] = signedUnorm(field.flow_x[i] / WATER_MAX_FLOW_SPEED);
    out[2] = signedUnorm(field.flow_y[i] / WATER_MAX_FLOW_SPEED);
    out[3] = unorm(field.land[i] / WATER_WET_TILES);
  });
}

}  // namespace eng
