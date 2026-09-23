#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <editor/shell/editor-ground-atlas.h>
#include <editor/shell/editor-terrains.h>
#include <engine/render-ground/ground-mesh.h>

using namespace eng;
using namespace eng::editor;

TEST_CASE("the atlas is one swatch per terrain, stacked") {
  const ImageData atlas = makeEditorGroundAtlas();
  REQUIRE(atlas.width == GROUND_SWATCH_TEXELS);
  REQUIRE(atlas.height == GROUND_SWATCH_TEXELS * EDITOR_TERRAIN_COUNT);
  REQUIRE(atlas.pixels.size() ==
          static_cast<size_t>(atlas.width) * atlas.height * 4);
}

TEST_CASE("each swatch stays within its terrain's grain of its colour") {
  const ImageData atlas = makeEditorGroundAtlas();
  for (size_t index = 0; index < EDITOR_TERRAIN_COUNT; ++index) {
    const EditorTerrain& terrain = EDITOR_TERRAINS[index];
    const size_t first = index * GROUND_SWATCH_TEXELS * GROUND_SWATCH_TEXELS;
    const size_t count = GROUND_SWATCH_TEXELS * GROUND_SWATCH_TEXELS;
    for (size_t texel = first; texel < first + count; ++texel) {
      REQUIRE(std::abs(atlas.pixels[texel * 4] - terrain.red) <= terrain.grain);
      REQUIRE(std::abs(atlas.pixels[texel * 4 + 2] - terrain.blue) <=
              terrain.grain);
      REQUIRE(atlas.pixels[texel * 4 + 3] == 255);
    }
  }
}

TEST_CASE("the atlas is the same every time it is made") {
  REQUIRE(makeEditorGroundAtlas().pixels == makeEditorGroundAtlas().pixels);
}
