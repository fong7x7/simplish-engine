#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-character-choices.h>
#include <editor/shell/editor-entity-id.h>
#include <string>
#include <vector>

using namespace eng::editor;
using eng::game::CharacterDefinition;

namespace {

/// Two characters, as a table would list them.
std::vector<CharacterDefinition> twoCharacters() {
  return {{"scout", "Scout", "", 7.0f, 3}, {"tank", "Tank", "", 3.0f, 9}};
}

}  // namespace

TEST_CASE("none is offered first, then every character by name") {
  const EditorCharacterChoices choices =
      editorCharacterChoices(twoCharacters(), "");

  REQUIRE(choices.names == std::vector<std::string>{"None", "Scout", "Tank"});
  REQUIRE(choices.refs ==
          std::vector<std::string>{"", "character:scout", "character:tank"});
  REQUIRE(choices.current == 0);
}

TEST_CASE("a start's character is the choice picked") {
  REQUIRE(editorCharacterChoices(twoCharacters(), "character:tank").current ==
          2);
}

TEST_CASE("a character the table no longer has is offered, last") {
  const EditorCharacterChoices choices =
      editorCharacterChoices(twoCharacters(), "character:gone");

  REQUIRE(choices.names.back() == "character:gone (missing)");
  REQUIRE(choices.refs.back() == "character:gone");
  REQUIRE(choices.current == choices.names.size() - 1);
}

TEST_CASE("a character reference names its id, and nothing else does") {
  REQUIRE(editorCharacterRef("scout") == "character:scout");
  REQUIRE(editorCharacterIdOf("character:scout") == "scout");
  REQUIRE(editorCharacterIdOf("mesh:scout").empty());
  REQUIRE(editorCharacterIdOf("").empty());
  REQUIRE(findEditorCharacter(twoCharacters(), "character:tank") == 1U);
  REQUIRE_FALSE(findEditorCharacter(twoCharacters(), "tank").has_value());
}

TEST_CASE("an asset is found by its reference") {
  std::vector<EditorAsset> assets(2);
  assets[0].name = "hero";
  assets[0].relative_path = "characters/hero.glb";
  assets[1].shape = EditorShapeKind::CUBE;
  assignEditorAssetIds(assets);

  REQUIRE(findEditorAssetByRef(assets, "mesh:characters_hero") == 0U);
  REQUIRE(findEditorAssetByRef(assets, "shape:cube") == 1U);
  REQUIRE_FALSE(findEditorAssetByRef(assets, "").has_value());
  REQUIRE_FALSE(findEditorAssetByRef(assets, "cube").has_value());
}
