#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-placement-clip.h>
#include <engine/animation/rig-pose.h>

using eng::animation::AnimationClip;
using eng::animation::Rig;
using eng::animation::RIG_REST_POSE;
using eng::editor::editorCharacterClip;
using eng::editor::EditorCharacterGait;
using eng::editor::editorClipIndex;
using eng::editor::editorClipNames;
using eng::editor::EditorPlacement;
using eng::editor::editorPlacementClip;

namespace {

/// A rig with three clips and no joints: all the clip helpers look at.
Rig threeClips() {
  Rig rig;
  rig.clips = {AnimationClip{"idle", 1.0f, {}}, AnimationClip{"walk", 1.0f, {}},
               AnimationClip{"run", 1.0f, {}}};
  return rig;
}

/// A placement playing @p clip.
EditorPlacement playing(std::string clip) {
  EditorPlacement placement;
  placement.animation = std::move(clip);
  return placement;
}

}  // namespace

TEST_CASE("a rig's clip names come in its own order") {
  const Rig rig = threeClips();
  REQUIRE(editorClipNames(&rig) ==
          std::vector<std::string>{"idle", "walk", "run"});
  REQUIRE(editorClipNames(nullptr).empty());
}

TEST_CASE("a placement plays the clip it names") {
  const Rig rig = threeClips();
  REQUIRE(editorPlacementClip(playing("run"), &rig) == 2);
}

TEST_CASE("a placement naming no clip, or a missing one, plays the first") {
  const Rig rig = threeClips();
  REQUIRE(editorPlacementClip(playing(""), &rig) == 0);
  REQUIRE(editorPlacementClip(playing("jump"), &rig) == 0);
  const std::vector<std::string> names = editorClipNames(&rig);
  REQUIRE(editorClipIndex(names, "jump") == 0);
}

TEST_CASE("a model with no clips, or no rig, stands at rest") {
  REQUIRE(editorPlacementClip(playing("walk"), nullptr) == RIG_REST_POSE);
  const Rig still;
  REQUIRE(editorPlacementClip(playing(""), &still) == RIG_REST_POSE);
}

TEST_CASE("a moving character runs, or walks when it has no run") {
  const std::vector<std::string> both{"Idle", "Walk_Loop", "Run_Fast"};
  const std::vector<std::string> walker{"Idle", "Walk_Loop"};
  REQUIRE(editorCharacterClip(both, EditorCharacterGait::MOVING) == "Run_Fast");
  REQUIRE(editorCharacterClip(walker, EditorCharacterGait::MOVING) ==
          "Walk_Loop");
}

TEST_CASE("a still character idles, matching the name in any case") {
  const std::vector<std::string> clips{"Walk", "IDLE_breathing"};
  REQUIRE(editorCharacterClip(clips, EditorCharacterGait::STILL) ==
          "IDLE_breathing");
}

TEST_CASE("a character with no clip named for its gait plays its first") {
  const std::vector<std::string> clips{"dance", "wave"};
  REQUIRE(editorCharacterClip(clips, EditorCharacterGait::STILL).empty());
  REQUIRE(editorCharacterClip(clips, EditorCharacterGait::MOVING).empty());
  REQUIRE(editorCharacterClip({}, EditorCharacterGait::MOVING).empty());
}
