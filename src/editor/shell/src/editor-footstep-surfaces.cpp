#include <editor/shell/editor-footstep-surfaces.h>
#include <editor/shell/editor-placement-transform.h>
#include <editor/shell/editor-terrains.h>

namespace eng::editor {

namespace {

  /// The surface terrain number @p terrain sounds like; bare ground for
  /// none.
  uint8_t surfaceOf(uint8_t terrain) {
    const EditorTerrain* found = editorTerrainAt(terrain);
    return static_cast<uint8_t>(
        found != nullptr ? found->surface : game::FootstepSurface::GROUND);
  }

  /// @p ground with every terrain number turned into its surface's.
  GroundGrid groundSurfaces(const GroundGrid& ground) {
    std::vector<uint8_t> cells = ground.cells();
    for (uint8_t& cell : cells) {
      cell = surfaceOf(cell);
    }
    return GroundGrid::fromCells(ground.bounds(), std::move(cells))
        .value_or(GroundGrid{});
  }

  /// @p surfaces with every cell under @p water sounding like water,
  /// whatever terrain it lies over.
  GroundGrid wadeThrough(GroundGrid surfaces, const WaterLayer& water) {
    const GroundRect bounds = waterLayerBounds(water);
    for (int32_t i = 0; i < bounds.width * bounds.height; ++i) {
      const GroundCell cell{bounds.x + i % bounds.width,
                            bounds.y + i / bounds.width};
      if (water.depth.at(cell) != 0) {
        surfaces.set(cell, static_cast<uint8_t>(game::FootstepSurface::WATER));
      }
    }
    return surfaces;
  }

}  // namespace

game::FootstepSurfaces
makeEditorFootstepSurfaces(const EditorDocument& document,
                           const std::vector<EditorAsset>& assets) {
  game::FootstepSurfaces level{
      .ground = wadeThrough(groundSurfaces(document.ground), document.water)};
  for (const EditorPlacement& placement : document.placements) {
    if (placement.surface && placement.asset < assets.size()) {
      const PlacementBounds box =
          placementWorldBounds(assets[placement.asset], placement);
      level.patches.push_back({box.min, box.max, *placement.surface});
    }
  }
  return level;
}

}  // namespace eng::editor
