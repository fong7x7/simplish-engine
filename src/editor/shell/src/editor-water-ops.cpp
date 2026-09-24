#include <algorithm>
#include <cmath>
#include <editor/shell/editor-water-ops.h>
#include <engine/render-ground/ground-region.h>
#include <utility>

namespace eng::editor {

namespace {

  /// @p value, 0 to 1, as the byte a water cell stores.
  uint8_t toByte(float value) {
    return static_cast<uint8_t>(
        std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
  }

  /// The byte of @p water @p field names.
  uint8_t* channelOf(WaterCell& water, EditorPropertyField field) {
    switch (field) {
      case EditorPropertyField::COLOR_R:
        return &water.red;
      case EditorPropertyField::COLOR_G:
        return &water.green;
      case EditorPropertyField::COLOR_B:
        return &water.blue;
      case EditorPropertyField::OPACITY:
        return &water.opacity;
      default:
        return nullptr;
    }
  }

  /// Change @p cell's water by @p change when there is water on it; false
  /// when it is dry or unchanged.
  template <typename Change>
  bool changeWet(WaterLayer& layer, GroundCell cell, Change change) {
    WaterCell water = waterCellAt(layer, cell);
    if (water.depth == 0) {
      return false;
    }
    change(water);
    return setWaterCell(layer, cell, water);
  }

  /// Call @p visit with every cell of @p rect, and whether any call said it
  /// changed something.
  template <typename Visit> bool eachCell(GroundRect rect, Visit visit) {
    bool changed = false;
    for (int32_t row = 0; row < rect.height; ++row) {
      for (int32_t column = 0; column < rect.width; ++column) {
        changed = visit(GroundCell{rect.x + column, rect.y + row}) || changed;
      }
    }
    return changed;
  }

  /// The smallest rectangle holding both @p a and @p b.
  GroundRect unionOf(const GroundRect& a, const GroundRect& b) {
    if (a.width == 0 || a.height == 0) {
      return b;
    }
    if (b.width == 0 || b.height == 0) {
      return a;
    }
    const int32_t x0 = std::min(a.x, b.x);
    const int32_t y0 = std::min(a.y, b.y);
    const int32_t x1 = std::max(a.x + a.width, b.x + b.width);
    const int32_t y1 = std::max(a.y + a.height, b.y + b.height);
    return {x0, y0, x1 - x0, y1 - y0};
  }

  /// The water layer's cells as 1 for water and 0 for dry.
  GroundGrid wetMask(const WaterLayer& layer) {
    std::vector<uint8_t> cells = layer.depth.cells();
    for (uint8_t& cell : cells) {
      cell = cell != 0 ? 1 : 0;
    }
    return GroundGrid::fromCells(layer.depth.bounds(), std::move(cells))
        .value_or(GroundGrid{});
  }

}  // namespace

bool layEditorWater(WaterLayer& layer, GroundRect rect,
                    const WaterCell& water) {
  return eachCell(rect, [&](GroundCell cell) {
    WaterCell laid = waterCellAt(layer, cell);
    laid = laid.depth == 0 ? water : laid;
    laid.depth = water.depth;
    return setWaterCell(layer, cell, laid);
  });
}

bool dryEditorWater(WaterLayer& layer, GroundRect rect) {
  return eachCell(rect, [&](GroundCell cell) {
    return setWaterCell(layer, cell, WaterCell{.depth = 0});
  });
}

bool setEditorWaterDepth(WaterLayer& layer, std::span<const GroundCell> cells,
                         uint8_t units) {
  bool changed = false;
  for (const GroundCell cell : cells) {
    changed = changeWet(layer, cell,
                        [&](WaterCell& water) { water.depth = units; }) ||
              changed;
  }
  return changed;
}

bool setEditorWaterDepthIn(WaterLayer& layer, GroundRect rect, uint8_t units) {
  return eachCell(rect, [&](GroundCell cell) {
    return changeWet(layer, cell,
                     [&](WaterCell& water) { water.depth = units; });
  });
}

bool setEditorWaterValue(WaterLayer& layer, std::span<const GroundCell> cells,
                         EditorPropertyField field, float value) {
  bool changed = false;
  for (const GroundCell cell : cells) {
    changed = changeWet(layer, cell,
                        [&](WaterCell& water) {
                          if (uint8_t* byte = channelOf(water, field)) {
                            *byte = toByte(value);
                          }
                        }) ||
              changed;
  }
  return changed;
}

float editorWaterValue(const WaterCell& water, EditorPropertyField field) {
  WaterCell copy = water;
  const uint8_t* byte = channelOf(copy, field);
  return byte != nullptr ? static_cast<float>(*byte) / 255.0f : 0.0f;
}

std::vector<GroundCell> connectedWaterCells(const WaterLayer& layer,
                                            GroundCell cell) {
  if (layer.depth.at(cell) == 0) {
    return {};
  }
  return connectedGroundCells(wetMask(layer), cell);
}

std::vector<EditorWaterChange> diffEditorWater(const WaterLayer& before,
                                               const WaterLayer& after) {
  std::vector<EditorWaterChange> changes;
  eachCell(unionOf(before.depth.bounds(), after.depth.bounds()),
           [&](GroundCell cell) {
             const WaterCell was = waterCellAt(before, cell);
             const WaterCell now = waterCellAt(after, cell);
             if (was != now) {
               changes.push_back({cell, was, now});
             }
             return false;
           });
  return changes;
}

void applyEditorWaterChanges(WaterLayer& layer,
                             std::span<const EditorWaterChange> changes,
                             EditorGroundSide side) {
  for (const EditorWaterChange& change : changes) {
    setWaterCell(layer, change.cell,
                 side == EditorGroundSide::AFTER ? change.after
                                                 : change.before);
  }
}

std::optional<EditorAction> editorWaterEdit(const EditorDocument& document,
                                            const WaterLayer& after) {
  std::vector<EditorWaterChange> changes =
      diffEditorWater(document.water, after);
  if (changes.empty()) {
    return std::nullopt;
  }
  return EditorAction{.kind = EditorActionKind::PAINT_GROUND,
                      .water = std::move(changes)};
}

}  // namespace eng::editor
