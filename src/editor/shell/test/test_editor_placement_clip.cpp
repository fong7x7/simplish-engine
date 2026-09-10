#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-placement-clip.h>
#include <engine/animation/rig-pose.h>

using eng::animation::AnimationClip;
using eng::animation::Rig;
using eng::animation::RIG_REST_POSE;
using eng::editor::editorClipIndex;
using eng::editor::editorClipNames;
using eng::editor::EditorPlacement;
using eng::editor::editorPlacementClip;
using eng::editor::stepEditorClip;

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

TEST_CASE("stepping through clips wraps round at either end") {
  const Rig rig = threeClips();
  const std::vector<std::string> clips = editorClipNames(&rig);
  REQUIRE(stepEditorClip(clips, "idle", 1) == "walk");
  REQUIRE(stepEditorClip(clips, "run", 1) == "idle");
  REQUIRE(stepEditorClip(clips, "idle", -1) == "run");
  REQUIRE(stepEditorClip(clips, "walk", 7) == "run");
}

TEST_CASE("stepping from no clip starts from the first") {
  const Rig rig = threeClips();
  REQUIRE(stepEditorClip(editorClipNames(&rig), "", 1) == "walk");
  REQUIRE(stepEditorClip({}, "idle", 1).empty());
}
