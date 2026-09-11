#include <editor/shell/editor-path-query.h>
#include <engine/spatial/nearest-open-cell.h>
#include <engine/spatial/path-finder.h>
#include <engine/spatial/path-smoothing.h>
#include <optional>

namespace eng::editor {

namespace {

  /// The length of walking @p waypoints from @p from.
  float walkedLength(Vec2 from, const std::vector<Vec2>& waypoints) {
    float length = 0.0F;
    for (const Vec2 point : waypoints) {
      length += Vec2::distance(from, point);
      from = point;
    }
    return length;
  }

  /// @p result's cells as smoothed waypoints.
  std::vector<Vec2> smoothed(const spatial::NavGrid& grid,
                             const spatial::PathResult& result,
                             uint8_t clearance) {
    std::vector<Vec2> waypoints(result.cells.size());
    waypoints.resize(
        spatial::smoothPath(grid, result.cells, clearance, waypoints));
    return waypoints;
  }

}  // namespace

EditorPathAnswer findEditorPath(const EditorNavigation& navigation,
                                const EditorPathQuery& query) {
  const spatial::NavGrid& grid = navigation.grid;
  const uint8_t clearance = grid.requiredClearance(query.radius);
  const auto from = editorStandingCell(grid, query.from, clearance);
  const auto to = editorStandingCell(grid, query.to, clearance);
  if (!from || !to) {
    return {.status = spatial::PathStatus::BLOCKED_ENDPOINT};
  }
  spatial::PathFinder finder(grid.cellCount());
  const spatial::PathResult result = finder.find(grid, {*from, *to, clearance});
  EditorPathAnswer answer{.status = result.status, .expanded = result.expanded};
  if (result.status == spatial::PathStatus::FOUND) {
    answer.waypoints = smoothed(grid, result, clearance);
    answer.length_tiles = walkedLength(grid.centre(*from), answer.waypoints);
  }
  return answer;
}

}  // namespace eng::editor
