#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-footstep-surfaces.h>
#include <editor/shell/editor-ground-ops.h>
#include <editor/shell/editor-shape.h>
#include <editor/shell/editor-terrains.h>
#include <engine/render-mesh/mesh-primitives.h>

using namespace eng;
using namespace eng::editor;

namespace {

/// The built-in shapes, measured as an upload would measure them.
std::vector<EditorAsset> measuredShapes() {
  std::vector<EditorAsset> assets;
  appendEditorShapeAssets(assets);
  for (EditorAsset& asset : assets) {
    const MeshData mesh = makeEditorShapeMesh(*asset.shape);
    asset.min = mesh.min;
    asset.max = mesh.max;
  }
  return assets;
}

/// Where the flat tile is among them.
size_t tileAsset(const std::vector<EditorAsset>& assets) {
  for (size_t i = 0; i < assets.size(); ++i) {
    if (assets[i].shape == EditorShapeKind::TILE) {
      return i;
    }
  }
  return 0;
}

}  // namespace

TEST_CASE("each painted cell sounds like its terrain") {
  EditorDocument document;
  paintEditorGround(document.ground, {0, 0, 1, 1}, *editorTerrainNamed("road"));
  paintEditorGround(document.ground, {1, 0, 1, 1},
                    *editorTerrainNamed("grass"));
  const game::FootstepSurfaces level = makeEditorFootstepSurfaces(document, {});

  REQUIRE(game::footstepSurfaceAt(level, {0.5F, 0.5F, 0}) ==
          game::FootstepSurface::STONE);
  REQUIRE(game::footstepSurfaceAt(level, {1.5F, 0.5F, 0}) ==
          game::FootstepSurface::GRASS);
  REQUIRE(game::footstepSurfaceAt(level, {5.5F, 0.5F, 0}) ==
          game::FootstepSurface::GROUND);
}

TEST_CASE("a rug laid on the grass is cloth underfoot, and nowhere else") {
  const std::vector<EditorAsset> assets = measuredShapes();
  EditorDocument document;
  paintEditorGround(document.ground, {0, 0, 6, 6},
                    *editorTerrainNamed("grass"));
  EditorPlacement rug{.asset = tileAsset(assets), .position = {2, 2, 0}};
  rug.surface = game::FootstepSurface::CLOTH;
  document.placements.push_back(rug);
  // A prop with no surface of its own leaves the ground heard.
  document.placements.push_back(
      {.asset = tileAsset(assets), .position = {4, 4, 0}});
  const game::FootstepSurfaces level =
      makeEditorFootstepSurfaces(document, assets);

  REQUIRE(level.patches.size() == 1);
  REQUIRE(game::footstepSurfaceAt(level, {2.5F, 2.5F, 0}) ==
          game::FootstepSurface::CLOTH);
  REQUIRE(game::footstepSurfaceAt(level, {4.5F, 4.5F, 0}) ==
          game::FootstepSurface::GRASS);
}
