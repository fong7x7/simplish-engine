#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-animation-event-table.h>

using namespace eng::editor;

namespace {

/// An animation events table holding @p entries, a JSON array's contents.
std::string table(const std::string& entries) {
  return R"({"schema": "simplish/data_table/1.0", "id": "animation_events",
             "content": {"entry_schema": "simplish/animation_events/1.0",
                         "entries": [)" +
         entries + "]}}";
}

}  // namespace

TEST_CASE("clip rows and sheet rows are read, with their events") {
  const EditorAnimationEventTable read =
      parseEditorAnimationEventTable(table(R"(
      {"asset": "mesh:knight", "clip": "walk",
       "events": [{"at": 0.1, "sound": "footstep"},
                  {"at": 0.5, "sound": "sounds/clank.wav", "gain": 0.5}]},
      {"sheet": "sprites/torch.png",
       "events": [{"frame": 2, "sound": "sounds/crackle.wav"}]})"));

  REQUIRE(read.problems.empty());
  REQUIRE(read.clips.size() == 1);
  REQUIRE(read.clips[0].events.size() == 2);
  REQUIRE(read.clips[0].events[1].gain == 0.5F);
  REQUIRE(read.sheets.size() == 1);
  REQUIRE(read.sheets[0].events[0].frame == 2);
}

TEST_CASE("rows naming nothing, repeats and soundless events are skipped") {
  const EditorAnimationEventTable read =
      parseEditorAnimationEventTable(table(R"(
      {"clip": "walk", "events": []},
      {"asset": "mesh:knight", "clip": "walk", "events": [{"at": 1}]},
      {"asset": "mesh:knight", "clip": "walk", "events": []},
      {"sheet": "a.png", "events": [{"frame": -3, "sound": "footstep",
                                     "gain": 99}]})"));

  REQUIRE(read.clips.size() == 1);
  REQUIRE(read.clips[0].events.empty());
  REQUIRE(read.problems.size() == 3);
  // Held to what makes sense rather than refused.
  REQUIRE(read.sheets[0].events[0].frame == 0);
  REQUIRE(read.sheets[0].events[0].gain == 4.0F);
}

TEST_CASE("the table survives being written and read back") {
  EditorAnimationEventTable table;
  table.clips.push_back({"mesh:knight", "run", {{0.2F, "footstep", 1.0F}}});
  table.sheets.push_back({"sprites/slime.png", {{1, "combat.blast", 0.8F}}});

  const EditorAnimationEventTable back =
      parseEditorAnimationEventTable(writeEditorAnimationEventTable(table));

  REQUIRE(back.clips == table.clips);
  REQUIRE(back.sheets == table.sheets);
}

TEST_CASE("a file that is not this table gives nothing, and says so") {
  const EditorAnimationEventTable read =
      parseEditorAnimationEventTable(R"({"schema": "nope"})");
  REQUIRE(read.clips.empty());
  REQUIRE(read.problems.size() == 1);
}
