#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-terrains.h>

using namespace eng::editor;

TEST_CASE("every terrain is found again by its own word") {
  for (size_t i = 0; i < EDITOR_TERRAIN_COUNT; ++i) {
    const auto number = static_cast<uint8_t>(i + 1);
    REQUIRE(editorTerrainNamed(EDITOR_TERRAINS[i].word) == number);
    REQUIRE(editorTerrainWord(number) == EDITOR_TERRAINS[i].word);
    REQUIRE(editorTerrainAt(number) == &EDITOR_TERRAINS[i]);
  }
}

TEST_CASE("a file's tile reference names the same terrain as its word") {
  REQUIRE(editorTerrainNamed("tile:sand") == editorTerrainNamed("sand"));
  REQUIRE(editorTerrainNamed("tile:none") == uint8_t{0});
  REQUIRE(editorTerrainNamed("none") == uint8_t{0});
}

TEST_CASE("a word no terrain has names nothing, and a number past the last "
          "has no word") {
  REQUIRE_FALSE(editorTerrainNamed("lava").has_value());
  REQUIRE_FALSE(editorTerrainNamed("").has_value());
  REQUIRE(editorTerrainAt(0) == nullptr);
  REQUIRE(editorTerrainAt(EDITOR_TERRAIN_COUNT + 1) == nullptr);
  REQUIRE(editorTerrainWord(EDITOR_TERRAIN_COUNT + 1).empty());
}

TEST_CASE("the road stacks over the sand it is laid through") {
  const std::optional<uint8_t> sand = editorTerrainNamed("sand");
  const std::optional<uint8_t> road = editorTerrainNamed("road");
  REQUIRE(sand.has_value());
  REQUIRE(road.has_value());
  REQUIRE(*road > *sand);
}
