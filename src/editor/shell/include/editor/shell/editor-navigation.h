#pragma once

/// @file editor-navigation.h
/// @brief The level's navigation grid, checked against its player starts
/// and its actors.
/// @par Threading Main-thread-only (pure functions over the document).

#include <cstdint>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-document.h>
#include <editor/shell/editor-nav-cell.h>
#include <engine/math/vec2.h>
#include <engine/spatial/grid-cell.h>
#include <engine/spatial/nav-grid.h>
#include <optional>
#include <string>
#include <vector>

namespace eng::editor {

/// What the navigation grid a playtest of a level would plan across says
/// about that level (Editor REQUIREMENTS §4.3: reachability, and warnings
/// for what cannot be reached).
///
/// Built by the same `game::buildWorldNavGrid` the game uses, from the same
/// setup a playtest would get — so what the overlay shows as a wall is what
/// the actors will walk round.
/// @thread_safety Main-thread-only.
struct EditorNavigation {
  /// The grid itself.
  spatial::NavGrid grid;
  /// The clearance an actor a player's width needs, which is what `cells`
  /// and the reachability in it are measured for.
  uint8_t clearance = 1;
  /// What each cell is, row-major.
  std::vector<EditorNavCell> cells;
  /// Whether the level has a player start to measure reachability from;
  /// without one nothing is marked unreachable, because nothing is known.
  bool reachability_known = false;
  /// The ids of actors no path joins to any player start.
  std::vector<std::string> unreachable_actors;
  /// The ids of actors with no floor near where they stand to start from —
  /// inside a prop, or wedged too tightly between props.
  std::vector<std::string> stranded_actors;
};

/// How many cells out a point looks for floor to stand on: two tiles, as an
/// actor planning a path does.
inline constexpr uint32_t EDITOR_NAV_SNAP_RINGS = 8;

/// The cell of @p grid nearest @p point that a walker needing @p clearance
/// can stand in, within `EDITOR_NAV_SNAP_RINGS`; nothing when there is none
/// that near, or @p point is off the grid.
[[nodiscard]] std::optional<spatial::GridCell>
editorStandingCell(const spatial::NavGrid& grid, Vec2 point, uint8_t clearance);

/// @p document's navigation, measured against @p assets.
[[nodiscard]] EditorNavigation
analyseEditorNavigation(const EditorDocument& document,
                        const std::vector<EditorAsset>& assets);

}  // namespace eng::editor
