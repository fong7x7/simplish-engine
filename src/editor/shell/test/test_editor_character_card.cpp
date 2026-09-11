#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-character-card.h>
#include <editor/shell/editor-entity-id.h>

using namespace eng::editor;

TEST_CASE("a card prints a character's stats, with its model's picture") {
  std::vector<EditorAsset> assets(1);
  assets[0].relative_path = "hero.glb";
  assignEditorAssetIds(assets);
  assets[0].thumbnail = eng::RhiTextureHandle{7};
  assets[0].thumbnail_state = EditorAssetThumbnailState::READY;

  const std::vector<EditorCharacterCard> cards =
      makeEditorCharacterCards({{"scout", "Scout", "mesh:hero", 6.5f, 4},
                                {"ghost", "Ghost", "mesh:gone", 5.0f, 1}},
                               assets);

  REQUIRE(cards.size() == 2);
  REQUIRE(cards[0].name == "Scout");
  REQUIRE(cards[0].speed == "Speed 6.5");
  REQUIRE(cards[0].health == "Health 4");
  REQUIRE(cards[0].picture == eng::RhiTextureHandle{7});
  // A model the project lacks has no picture.
  REQUIRE(cards[1].picture == eng::RHI_TEXTURE_INVALID);
}
