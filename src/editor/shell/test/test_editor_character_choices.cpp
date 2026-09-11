#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-character-choices.h>
#include <editor/shell/editor-entity-id.h>
#include <string>
#include <vector>

using namespace eng::editor;

namespace {

/// A model on disk and a built-in shape, with the ids a scan gives them.
std::vector<EditorAsset> twoAssets() {
  std::vector<EditorAsset> assets(2);
  assets[0].name = "hero";
  assets[0].relative_path = "characters/hero.glb";
  assets[1].name = "Cube";
  assets[1].shape = EditorShapeKind::CUBE;
  assignEditorAssetIds(assets);
  return assets;
}

}  // namespace

TEST_CASE("the stand-in is offered first, then every asset") {
  const EditorCharacterChoices choices =
      editorCharacterChoices(twoAssets(), "");

  REQUIRE(choices.names ==
          std::vector<std::string>{"Stand-in", "hero", "Cube"});
  REQUIRE(choices.refs ==
          std::vector<std::string>{"", "mesh:characters_hero", "shape:cube"});
  REQUIRE(choices.current == 0);
}

TEST_CASE("a start's character is the choice picked") {
  const EditorCharacterChoices choices =
      editorCharacterChoices(twoAssets(), "shape:cube");
  REQUIRE(choices.current == 2);
}

TEST_CASE("a character the project no longer has is offered, last") {
  const EditorCharacterChoices choices =
      editorCharacterChoices(twoAssets(), "mesh:gone");

  REQUIRE(choices.names.back() == "mesh:gone (missing)");
  REQUIRE(choices.refs.back() == "mesh:gone");
  REQUIRE(choices.current == choices.names.size() - 1);
}

TEST_CASE("a character is found by its reference, and only by that") {
  const std::vector<EditorAsset> assets = twoAssets();
  REQUIRE(findEditorCharacterAsset(assets, "mesh:characters_hero") == 0U);
  REQUIRE_FALSE(findEditorCharacterAsset(assets, "").has_value());
  REQUIRE_FALSE(findEditorCharacterAsset(assets, "mesh:gone").has_value());
  REQUIRE_FALSE(findEditorCharacterAsset(assets, "cube").has_value());
}
