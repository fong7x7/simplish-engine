#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/render-water/water-corners.h>
#include <engine/render-water/water-depth.h>

using Catch::Approx;
using namespace eng;

namespace {

/// A strip of water @p length cells long along x from the origin: the west
/// half @p west tiles deep and red, the east half @p east deep and blue.
WaterLayer strip(int32_t length, float west, float east) {
  WaterLayer layer;
  for (int32_t x = 0; x < length; ++x) {
    const bool west_half = x < length / 2;
    setWaterCell(layer, {x, 0},
                 {waterDepthUnits(west_half ? west : east),
                  static_cast<uint8_t>(west_half ? 255 : 0), 0,
                  static_cast<uint8_t>(west_half ? 0 : 255), 128});
  }
  return layer;
}

}  // namespace

TEST_CASE("a depth is stored in steps, and zero is dry",
          "[render-water][depth]") {
  CHECK(waterDepthTiles(0) == 0.0f);
  CHECK(waterDepthTiles(waterDepthUnits(3.0f)) == 3.0f);
  CHECK(waterDepthTiles(waterDepthUnits(0.0625f)) == 0.0625f);
  CHECK(waterDepthUnits(0.0f) == 1);
  CHECK(waterDepthUnits(1000.0f) == 255);
  CHECK(waterDepthTiles(255) == WATER_MAX_DEPTH);
}

TEST_CASE("water shelves from nothing at a bank to its full depth",
          "[render-water][depth]") {
  CHECK(waterBankShelf(1.0f, 0.0f) == 0.0f);
  CHECK(waterBankShelf(1.0f, WATER_BANK_MAX_TILES) == 1.0f);
  CHECK(waterBankShelf(0.1f, 0.4f) == 1.0f);
  // A lake takes further to reach its depth than a puddle.
  CHECK(waterBankShelf(4.0f, 0.5f) < waterBankShelf(0.1f, 0.2f));
}

TEST_CASE("depth and colour blend across the tile between two waters",
          "[render-water][depth]") {
  const WaterLayer layer = strip(8, 0.25f, 3.0f);
  const WaterCorners corners = makeWaterCorners(layer, waterLayerBounds(layer));
  // Away from where they meet, each side is its own water: a corner on the
  // strip's edge still touches water, and it is the bank's shelf, not the
  // corners, that takes the depth to nothing there.
  const WaterSample west = waterSampleAt(corners, {1.0f, 0.5f});
  const WaterSample east = waterSampleAt(corners, {6.0f, 0.5f});
  CHECK(west.depth == Approx(0.25f));
  CHECK(east.depth == Approx(3.0f));
  CHECK(west.color.x == Approx(1.0f));
  CHECK(east.color.z == Approx(1.0f));
  CHECK(west.opacity == Approx(128.0f / 255.0f));
  // Across the corner line where the two meet it runs smoothly between.
  const WaterSample meet = waterSampleAt(corners, {4.0f, 0.5f});
  CHECK(meet.depth > waterSampleAt(corners, {3.5f, 0.5f}).depth);
  CHECK(meet.depth < waterSampleAt(corners, {4.5f, 0.5f}).depth);
  CHECK(meet.color.x > 0.0f);
  CHECK(meet.color.z > 0.0f);
}

TEST_CASE("a dry corner lowers depth without darkening the colour",
          "[render-water][depth]") {
  WaterLayer layer;
  setWaterCell(layer, {0, 0}, {16, 255, 255, 255, 255});
  const WaterCorners corners = makeWaterCorners(layer, {-1, -1, 3, 3});
  CHECK(waterSampleAt(corners, {0.0f, 0.0f}).depth ==
        Approx(WATER_DEFAULT_DEPTH));
  CHECK(waterSampleAt(corners, {-0.5f, -0.5f}).color.x == Approx(1.0f));
  CHECK(waterSampleAt(corners, {-1.0f, -1.0f}).depth == 0.0f);
  CHECK(waterSampleAt(corners, {9.0f, 9.0f}).depth == 0.0f);
}
