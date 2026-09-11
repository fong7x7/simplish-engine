#include <algorithm>
#include <editor/shell/editor-actor-placement.h>
#include <editor/shell/editor-navigation.h>
#include <editor/shell/editor-playtest-session.h>
#include <engine/spatial/nearest-open-cell.h>
#include <engine/spatial/reachability.h>
#include <game/actors/actor-spawn.h>
#include <game/world/world-nav-grid.h>
#include <optional>

namespace eng::editor {

namespace {

  /// The setup a playtest would get, with every player's first start in it
  /// so the grid is fitted round all of them, not only player 1's.
  game::GameSetup navigationSetup(const EditorDocument& document,
                                  const std::vector<EditorAsset>& assets) {
    game::GameSetup setup = makeEditorPlaytestSetup(document, assets, {});
    for (const EditorPlayerStart& start : document.player_starts) {
      const auto slot = static_cast<uint8_t>(start.player - 1U);
      if (slot >= setup.player_count && slot < sim::MAX_PLAYERS) {
        setup.spawns[slot] = {start.position.x, start.position.y, 0.0F};
        setup.player_count = static_cast<uint8_t>(slot + 1U);
      }
    }
    return setup;
  }

  /// Where every player start stands on @p grid, for @p clearance.
  std::vector<spatial::GridCell> startCells(const spatial::NavGrid& grid,
                                            const EditorDocument& document,
                                            uint8_t clearance) {
    std::vector<spatial::GridCell> cells;
    for (const EditorPlayerStart& start : document.player_starts) {
      const Vec2 at{start.position.x, start.position.y};
      if (const auto cell = editorStandingCell(grid, at, clearance)) {
        cells.push_back(*cell);
      }
    }
    return cells;
  }

  /// What the cell at @p index is, given what is reachable.
  EditorNavCell classify(const EditorNavigation& navigation,
                         const std::vector<uint8_t>& reached, uint32_t index) {
    const uint8_t clearance =
        navigation.grid.clearance(navigation.grid.cellOf(index));
    if (clearance == 0) {
      return EditorNavCell::SOLID;
    }
    if (clearance < navigation.clearance) {
      return EditorNavCell::NARROW;
    }
    return navigation.reachability_known && reached[index] == 0
               ? EditorNavCell::UNREACHABLE
               : EditorNavCell::OPEN;
  }

  /// Whether an actor standing at @p cell, needing @p clearance, can reach
  /// any of @p document's player starts.
  bool reachesAStart(const spatial::NavGrid& grid, spatial::GridCell cell,
                     uint8_t clearance, const EditorDocument& document) {
    const std::vector<uint8_t> reached =
        spatial::reachableCells(grid, std::span(&cell, 1), clearance);
    return std::ranges::any_of(startCells(grid, document, clearance),
                               [&](spatial::GridCell start) {
                                 return reached[grid.indexOf(start)] != 0;
                               });
  }

  /// Sort the actor @p spawn — the prop @p id — into stranded, unreachable,
  /// or neither.
  void checkActor(EditorNavigation& navigation, const game::ActorSpawn& spawn,
                  const std::string& id, const EditorDocument& document) {
    const uint8_t clearance = navigation.grid.requiredClearance(spawn.radius);
    const auto cell = editorStandingCell(navigation.grid,
                                         {spawn.at.x, spawn.at.y}, clearance);
    if (!cell) {
      navigation.stranded_actors.push_back(id);
    } else if (navigation.reachability_known &&
               !reachesAStart(navigation.grid, *cell, clearance, document)) {
      navigation.unreachable_actors.push_back(id);
    }
  }

  /// Every cell's kind, measured from @p document's starts.
  std::vector<EditorNavCell> classifyAll(const EditorNavigation& navigation,
                                         const EditorDocument& document) {
    const std::vector<spatial::GridCell> starts =
        startCells(navigation.grid, document, navigation.clearance);
    const std::vector<uint8_t> reached =
        spatial::reachableCells(navigation.grid, starts, navigation.clearance);
    std::vector<EditorNavCell> cells(navigation.grid.cellCount());
    for (uint32_t i = 0; i < navigation.grid.cellCount(); ++i) {
      cells[i] = classify(navigation, reached, i);
    }
    return cells;
  }

}  // namespace

std::optional<spatial::GridCell>
editorStandingCell(const spatial::NavGrid& grid, Vec2 point,
                   uint8_t clearance) {
  const auto cell = grid.cellAt(point);
  if (!cell) {
    return std::nullopt;
  }
  return spatial::nearestOpenCell(grid, *cell, clearance,
                                  EDITOR_NAV_SNAP_RINGS);
}

EditorNavigation
analyseEditorNavigation(const EditorDocument& document,
                        const std::vector<EditorAsset>& assets) {
  const game::GameSetup setup = navigationSetup(document, assets);
  EditorNavigation navigation;
  navigation.grid = game::buildWorldNavGrid(setup);
  navigation.clearance =
      navigation.grid.requiredClearance(game::ACTOR_DEFAULT_RADIUS_TILES);
  navigation.reachability_known = !document.player_starts.empty();
  navigation.cells = classifyAll(navigation, document);
  const std::vector<std::string> ids = editorActorIds(document);
  for (size_t k = 0; k < setup.actors.size() && k < ids.size(); ++k) {
    checkActor(navigation, setup.actors[k], ids[k], document);
  }
  return navigation;
}

}  // namespace eng::editor
