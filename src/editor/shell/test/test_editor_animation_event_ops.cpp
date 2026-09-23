#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-animation-event-ops.h>
#include <engine/audio/audio-synth.h>

using namespace eng;
using namespace eng::editor;
using namespace eng::animation;

namespace {

/// A hip and a foot that lifts and comes down once a second in "walk", and
/// stands still in "idle".
Rig walkingRig() {
  Rig rig;
  rig.skeleton.parents = {SKELETON_NO_PARENT, 0};
  rig.skeleton.rest = {JointPose{.translation = {0, 0, 1}},
                       JointPose{.translation = {0, 0, -1}}};
  rig.skeleton.names = {"Hips", "LeftFoot"};
  AnimationChannel lift;
  lift.joint = 1;
  lift.times = {0.0F, 0.5F, 1.0F};
  lift.values = {0, 0, 0, 0, 0, -1, 0, 0, 0};
  rig.clips.push_back({"walk", 1.0F, {lift}});
  rig.clips.push_back({"idle", 1.0F, {}});
  return rig;
}

}  // namespace

TEST_CASE("a clip nobody wrote events for steps where its foot comes down") {
  const auto sets = resolveEditorClipEvents({}, "mesh:knight", walkingRig());
  REQUIRE(sets.size() == 2);
  REQUIRE(sets[0].source == EditorEventSource::DETECTED);
  REQUIRE_FALSE(sets[0].events.empty());
  REQUIRE(sets[0].events[0].sound == EDITOR_FOOTSTEP_EVENT);
  REQUIRE(editorEventsStep(sets[0]));
  // An idle's feet never lift.
  REQUIRE(sets[1].source == EditorEventSource::NONE);
}

TEST_CASE("a clip the table names plays the table's events, in time order") {
  EditorAnimationEventTable table;
  table.clips.push_back(
      {"mesh:knight",
       "walk",
       {{0.7F, "sounds/b.wav", 1.0F}, {0.1F, "sounds/a.wav", 1.0F}}});
  // Another model's row is none of this one's business.
  table.clips.push_back({"mesh:other", "idle", {{0.0F, "footstep", 1.0F}}});
  const auto sets = resolveEditorClipEvents(table, "mesh:knight", walkingRig());

  REQUIRE(sets[0].source == EditorEventSource::AUTHORED);
  REQUIRE(sets[0].events[0].sound == "sounds/a.wav");
  REQUIRE_FALSE(editorEventsStep(sets[0]));
  REQUIRE(sets[1].source == EditorEventSource::NONE);
}

TEST_CASE("an event names a file unless it is a footstep or a slot") {
  REQUIRE(editorEventNamesFile("sounds/swoosh.wav"));
  REQUIRE_FALSE(editorEventNamesFile("footstep"));
  REQUIRE_FALSE(editorEventNamesFile("combat.blast"));
  REQUIRE_FALSE(editorEventNamesFile("step.boots.wood"));
}

TEST_CASE("a slot's clip is found by its name, a file's once loaded") {
  audio::AudioClipBank bank;
  const auto blast = bank.add("combat.blast", audio::synthesize({}, 48000));
  REQUIRE(findEditorEventClip(bank, "combat.blast") == blast);
  REQUIRE_FALSE(findEditorEventClip(bank, "sounds/gone.wav").has_value());

  EditorAnimationEventTable table;
  table.clips.push_back(
      {"mesh:knight", "walk", {{0.0F, "sounds/gone.wav", 1.0F}}});
  const auto problems = loadEditorEventSounds(bank, table, "/no/such/assets");
  REQUIRE(problems.size() == 1);
}
