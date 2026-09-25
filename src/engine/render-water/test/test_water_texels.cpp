#include <catch2/catch_test_macros.hpp>
#include <engine/render-water/water-texels.h>

using namespace eng;

namespace {

/// A field over a @p side × @p side pond at @p samples_per_tile.
WaterField pondField(int32_t side, uint32_t samples_per_tile) {
  WaterLayer layer;
  for (int32_t y = 0; y < side; ++y) {
    for (int32_t x = 0; x < side; ++x) {
      setWaterCell(layer, {x, y}, {.depth = 16});
    }
  }
  WaterField field;
  resetWaterField(field, layer, samples_per_tile);
  return field;
}

/// The texel over the sample nearest @p at.
const uint8_t* texelNear(const WaterField& field,
                         const std::vector<uint8_t>& texels, Vec2 at) {
  const auto n = static_cast<float>(field.samples_per_tile);
  const auto x =
      static_cast<size_t>((at.x - static_cast<float>(field.origin.x)) * n);
  const auto y =
      static_cast<size_t>((at.y - static_cast<float>(field.origin.y)) * n);
  return texels.data() + (y * field.width + x) * WATER_TEXEL_BYTES;
}

}  // namespace

TEST_CASE("still water packs as level, unsloped texels",
          "[render-water][texels]") {
  const WaterField field = pondField(8, 4);
  std::vector<uint8_t> texels;
  writeWaterTexels(field, texels);
  REQUIRE(texels.size() == field.level.size() * WATER_TEXEL_BYTES);
  const uint8_t* deep = texelNear(field, texels, {4.0f, 4.0f});
  CHECK(deep[0] == 128);
  CHECK(deep[1] == 128);
  CHECK(deep[2] == 128);
  CHECK(deep[3] == 0);
}

TEST_CASE("the still texels hold the shore, the land, no flow, water",
          "[render-water][texels]") {
  const WaterField field = pondField(8, 4);
  std::vector<uint8_t> texels;
  writeWaterStillTexels(field, texels);
  REQUIRE(texels.size() == field.level.size() * WATER_TEXEL_BYTES);
  const uint8_t* deep = texelNear(field, texels, {4.0f, 4.0f});
  CHECK(deep[0] == 255);
  CHECK(deep[1] == 128);
  CHECK(deep[2] == 128);
  CHECK(deep[3] == 0);
  // Shallow water above the middle; wet land beside it just below; dry
  // land a tile out at the bottom.
  CHECK(texelNear(field, texels, {0.1f, 4.0f})[0] > 128);
  CHECK(texelNear(field, texels, {0.1f, 4.0f})[0] < 255);
  CHECK(texelNear(field, texels, {-0.1f, 4.0f})[0] < 128);
  CHECK(texelNear(field, texels, {-0.1f, 4.0f})[0] > 0);
  CHECK(texelNear(field, texels, {-0.9f, 4.0f})[0] == 0);
}

// Req: docs/engine/water.md §2 — how thick the water is reaches the shader.
TEST_CASE("the still texels hold how thick the water is",
          "[render-water][texels]") {
  WaterLayer layer;
  setWaterCell(layer, {0, 0}, {.depth = 16, .viscosity = 255});
  WaterField field;
  resetWaterField(field, layer, 4);
  std::vector<uint8_t> texels;
  writeWaterStillTexels(field, texels);
  CHECK(texelNear(field, texels, {0.5f, 0.5f})[3] == 255);
}

TEST_CASE("a hollow slopes up away from its middle on both axes",
          "[render-water][texels]") {
  WaterField field = pondField(8, 8);
  disturbWaterField(field, {4.0f, 4.0f}, 0.6f, 0.02f);
  std::vector<uint8_t> texels;
  writeWaterTexels(field, texels);
  CHECK(texelNear(field, texels, {4.3f, 4.0f})[0] > 128);
  CHECK(texelNear(field, texels, {3.7f, 4.0f})[0] < 128);
  CHECK(texelNear(field, texels, {4.0f, 4.3f})[1] > 128);
  CHECK(texelNear(field, texels, {4.0f, 3.7f})[1] < 128);
  CHECK(texelNear(field, texels, {4.0f, 4.0f})[2] < 128);
}
