#include <engine/render-ground/ground-quarter-shape.h>

namespace eng {

namespace {

  /// Whether @p cell counts as painted for @p layer: it holds that layer
  /// or one stacked above it.
  bool covers(const GroundGrid& grid, uint8_t layer, GroundCell cell) {
    return layer != 0 && grid.at(cell) >= layer;
  }

}  // namespace

GroundQuarterShape groundQuarterShape(const GroundGrid& grid, uint8_t layer,
                                      GroundCell cell, GroundQuarter quarter) {
  const int32_t dx = groundQuarterDx(quarter);
  const int32_t dy = groundQuarterDy(quarter);
  const bool across_x = covers(grid, layer, {cell.x + dx, cell.y});
  const bool across_y = covers(grid, layer, {cell.x, cell.y + dy});
  if (covers(grid, layer, cell)) {
    const bool diagonal = covers(grid, layer, {cell.x + dx, cell.y + dy});
    return across_x || across_y || diagonal ? GroundQuarterShape::FULL
                                            : GroundQuarterShape::ROUND;
  }
  return across_x && across_y ? GroundQuarterShape::FILLET
                              : GroundQuarterShape::EMPTY;
}

}  // namespace eng
