#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-event-hits.h>
#include <memory>

using namespace eng;
using namespace eng::editor;

namespace {

/// An asset with a one-second clip whose events are @p events.
std::vector<EditorAsset> assetWith(std::vector<EditorAnimationEvent> events) {
  auto rig = std::make_shared<animation::Rig>();
  rig->clips.push_back({"walk", 1.0F, {}});
  std::vector<EditorAsset> assets(1);
  assets[0].rig = rig;
  assets[0].clip_events = {{std::move(events), EditorEventSource::AUTHORED}};
  return assets;
}

/// A billboard showing @p sheet, four frames at @p fps.
EditorSprite billboard(const char* sheet, float fps) {
  EditorSprite sprite;
  sprite.id = "sprite_01";
  sprite.sheet = sheet;
  sprite.position = {3.0F, 4.0F, 0.0F};
  sprite.grid = {.columns = 4, .rows = 1, .frames = 4, .fps = fps};
  return sprite;
}

}  // namespace

TEST_CASE("a clip's events are hit as its pass reaches them, where it is") {
  const auto assets =
      assetWith({{0.25F, "footstep", 1.0F}, {0.75F, "sounds/clank.wav", 0.5F}});
  std::vector<EditorEventHit> hits;
  appendClipEventHits({"player:1", {1, 2, 0}, 0, 0, {0.5, 1.3}}, assets, hits);

  // Across the loop: the clank late in this one, the step early in the next.
  REQUIRE(hits.size() == 2);
  REQUIRE(hits[0].sound == "sounds/clank.wav");
  REQUIRE(hits[0].gain == 0.5F);
  REQUIRE(hits[1].sound == "footstep");
  REQUIRE(hits[1].key == "player:1");
  REQUIRE(hits[1].at.y == 2.0F);
}

TEST_CASE("a pass for a model or clip that is not there hits nothing") {
  const auto assets = assetWith({{0.25F, "footstep", 1.0F}});
  std::vector<EditorEventHit> hits;
  appendClipEventHits({"x", {}, 5, 0, {0.0, 1.0}}, assets, hits);
  appendClipEventHits({"x", {}, 0, 3, {0.0, 1.0}}, assets, hits);
  REQUIRE(hits.empty());
}

TEST_CASE("a sheet's frame event is hit as the billboard reaches the frame") {
  EditorAnimationEventTable table;
  table.sheets.push_back(
      {"sprites/torch.png", {{2, "sounds/crackle.wav", 1.0F}, {9, "x", 1.0F}}});
  std::vector<EditorEventHit> hits;
  // Four frames a second: frame 2 comes up at half a second.
  appendSheetEventHits(billboard("sprites/torch.png", 4.0F), table, {0.4, 0.6},
                       hits);
  REQUIRE(hits.size() == 1);
  REQUIRE(hits[0].sound == "sounds/crackle.wav");
  REQUIRE(hits[0].key == "sprite_01");
  REQUIRE(hits[0].at.x == 3.0F);
}

TEST_CASE("a sheet's event is not hit again until the sheet comes round, "
          "nor for a frame it lacks or a billboard held still") {
  EditorAnimationEventTable table;
  table.sheets.push_back(
      {"sprites/torch.png", {{2, "sounds/crackle.wav", 1.0F}, {9, "x", 1.0F}}});
  std::vector<EditorEventHit> hits;
  appendSheetEventHits(billboard("sprites/torch.png", 4.0F), table, {0.6, 1.4},
                       hits);
  appendSheetEventHits(billboard("sprites/torch.png", 0.0F), table, {0.0, 5.0},
                       hits);
  appendSheetEventHits(billboard("sprites/other.png", 4.0F), table, {0.0, 5.0},
                       hits);
  REQUIRE(hits.empty());
}
