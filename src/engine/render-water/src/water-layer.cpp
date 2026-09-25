#include <cmath>
#include <engine/render-water/water-layer.h>
#include <numbers>

namespace eng {

namespace {

  /// A dry cell: every byte zero.
  constexpr WaterCell DRY{0, 0, 0, 0, 0, 0, 0, 0};

}  // namespace

WaterCell waterCellAt(const WaterLayer& layer, GroundCell cell) {
  const uint8_t depth = layer.depth.at(cell);
  if (depth == 0) {
    return DRY;
  }
  return {depth,
          layer.red.at(cell),
          layer.green.at(cell),
          layer.blue.at(cell),
          layer.opacity.at(cell),
          layer.flow_heading.at(cell),
          layer.flow_speed.at(cell),
          layer.viscosity.at(cell)};
}

bool setWaterCell(WaterLayer& layer, GroundCell cell, const WaterCell& water) {
  const WaterCell stored = water.depth == 0 ? DRY : water;
  bool changed = layer.depth.set(cell, stored.depth);
  changed = layer.red.set(cell, stored.red) || changed;
  changed = layer.green.set(cell, stored.green) || changed;
  changed = layer.blue.set(cell, stored.blue) || changed;
  changed = layer.opacity.set(cell, stored.opacity) || changed;
  changed = layer.flow_heading.set(cell, stored.flow_heading) || changed;
  changed = layer.flow_speed.set(cell, stored.flow_speed) || changed;
  changed = layer.viscosity.set(cell, stored.viscosity) || changed;
  return changed;
}

Vec2 waterCellFlow(const WaterCell& water) {
  const float turn = static_cast<float>(water.flow_heading) *
                     (2.0f * std::numbers::pi_v<float> / 256.0f);
  const float speed =
      static_cast<float>(water.flow_speed) * (WATER_MAX_FLOW_SPEED / 255.0f);
  return {std::cos(turn) * speed, std::sin(turn) * speed};
}

GroundRect waterLayerBounds(const WaterLayer& layer) {
  return layer.depth.paintedBounds();
}

}  // namespace eng
