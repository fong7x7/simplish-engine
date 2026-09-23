#include <algorithm>
#include <cmath>
#include <editor/shell/editor-ground-ops.h>

namespace eng::editor {

namespace {

  /// The rectangle holding both @p a and @p b. An empty one holds nothing.
  GroundRect unionOf(const GroundRect& a, const GroundRect& b) {
    if (a.width <= 0 || a.height <= 0) {
      return b;
    }
    if (b.width <= 0 || b.height <= 0) {
      return a;
    }
    const int32_t x0 = std::min(a.x, b.x);
    const int32_t y0 = std::min(a.y, b.y);
    return {x0, y0, std::max(a.x + a.width, b.x + b.width) - x0,
            std::max(a.y + a.height, b.y + b.height) - y0};
  }

  /// @p value as a whole number of tiles, held inside the grid's limit so
  /// a wild float cannot overflow the conversion.
  int32_t tileOf(float value) {
    const auto limit = static_cast<float>(GROUND_COORDINATE_LIMIT);
    return static_cast<int32_t>(std::floor(std::clamp(value, -limit, limit)));
  }

}  // namespace

GroundRect editorBrushRect(WorldPoint point, int32_t size) {
  const int32_t side =
      std::clamp(size, EDITOR_BRUSH_SIZE_MIN, EDITOR_BRUSH_SIZE_MAX);
  const int32_t reach = side / 2;
  return {tileOf(point.x) - reach, tileOf(point.y) - reach, side, side};
}

bool paintEditorGround(GroundGrid& grid, GroundRect rect, uint8_t terrain) {
  bool changed = false;
  for (int32_t row = 0; row < rect.height; ++row) {
    for (int32_t column = 0; column < rect.width; ++column) {
      changed = grid.set({rect.x + column, rect.y + row}, terrain) || changed;
    }
  }
  return changed;
}

std::vector<EditorGroundChange> diffEditorGround(const GroundGrid& before,
                                                 const GroundGrid& after) {
  const GroundRect span = unionOf(before.bounds(), after.bounds());
  std::vector<EditorGroundChange> changes;
  for (int32_t row = 0; row < span.height; ++row) {
    for (int32_t column = 0; column < span.width; ++column) {
      const GroundCell cell{span.x + column, span.y + row};
      if (before.at(cell) != after.at(cell)) {
        changes.push_back({cell, before.at(cell), after.at(cell)});
      }
    }
  }
  return changes;
}

void applyEditorGroundChanges(GroundGrid& grid,
                              std::span<const EditorGroundChange> changes,
                              EditorGroundSide side) {
  for (const EditorGroundChange& change : changes) {
    grid.set(change.cell,
             side == EditorGroundSide::AFTER ? change.after : change.before);
  }
}

}  // namespace eng::editor
