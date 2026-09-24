#include <catch2/catch_test_macros.hpp>
#include <engine/render-water/water-layer.h>

using namespace eng;

TEST_CASE("water laid on a cell reads back, and a dry cell reads as none",
          "[render-water][layer]") {
  WaterLayer layer;
  const WaterCell lake{48, 20, 40, 80, 200};
  CHECK(setWaterCell(layer, {3, -2}, lake));
  CHECK(waterCellAt(layer, {3, -2}) == lake);
  CHECK(waterCellAt(layer, {0, 0}).depth == 0);
  CHECK_FALSE(setWaterCell(layer, {3, -2}, lake));
  CHECK(waterLayerBounds(layer) == GroundRect{3, -2, 1, 1});
}

TEST_CASE("drying a cell clears every byte of it", "[render-water][layer]") {
  WaterLayer layer;
  setWaterCell(layer, {1, 1}, {.depth = 16});
  CHECK(setWaterCell(layer, {1, 1}, {.depth = 0}));
  CHECK(waterCellAt(layer, {1, 1}) == WaterCell{0, 0, 0, 0, 0});
  CHECK(layer.red.at({1, 1}) == 0);
  CHECK(waterLayerBounds(layer).width == 0);
}
