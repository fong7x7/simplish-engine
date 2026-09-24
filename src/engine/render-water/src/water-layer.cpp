#include <engine/render-water/water-layer.h>

namespace eng {

WaterCell waterCellAt(const WaterLayer& layer, GroundCell cell) {
  const uint8_t depth = layer.depth.at(cell);
  if (depth == 0) {
    return {0, 0, 0, 0, 0};
  }
  return {depth, layer.red.at(cell), layer.green.at(cell), layer.blue.at(cell),
          layer.opacity.at(cell)};
}

bool setWaterCell(WaterLayer& layer, GroundCell cell, const WaterCell& water) {
  const WaterCell stored = water.depth == 0 ? WaterCell{0, 0, 0, 0, 0} : water;
  bool changed = layer.depth.set(cell, stored.depth);
  changed = layer.red.set(cell, stored.red) || changed;
  changed = layer.green.set(cell, stored.green) || changed;
  changed = layer.blue.set(cell, stored.blue) || changed;
  changed = layer.opacity.set(cell, stored.opacity) || changed;
  return changed;
}

GroundRect waterLayerBounds(const WaterLayer& layer) {
  return layer.depth.paintedBounds();
}

}  // namespace eng
