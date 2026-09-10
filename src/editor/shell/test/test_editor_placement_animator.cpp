#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-placement-animator.h>
#include <engine/math/math.h>

using Catch::Approx;
using eng::Mat4;
using eng::animation::AnimationChannel;
using eng::animation::AnimationClip;
using eng::animation::ChannelTarget;
using eng::animation::Rig;
using eng::animation::SKELETON_NO_PARENT;
using eng::editor::EditorPlacement;
using eng::editor::EditorPlacementAnimator;

namespace {

/// A clip holding the one joint at @p x along X.
AnimationClip holdAt(const char* name, float x) {
  AnimationChannel c;
  c.target = ChannelTarget::TRANSLATION;
  c.times = {0.0f, 1.0f};
  c.values = {x, 0.0f, 0.0f, x, 0.0f, 0.0f};
  return {name, 1.0f, {c}};
}

/// One joint with two clips: "left" at -1 and "right" at +1.
Rig leftRight() {
  Rig rig;
  rig.skeleton.parents = {SKELETON_NO_PARENT};
  rig.skeleton.rest.resize(1);
  rig.skeleton.names = {"joint"};
  rig.skin.joints = {0};
  rig.skin.inverse_bind = {Mat4::identity()};
  rig.clips = {holdAt("left", -1.0f), holdAt("right", 1.0f)};
  return rig;
}

/// A placement with id @p id playing @p clip.
EditorPlacement prop(const char* id, const char* clip) {
  EditorPlacement placement;
  placement.id = id;
  placement.animation = clip;
  return placement;
}

/// Where @p placement's joint is at @p now.
float xAt(EditorPlacementAnimator& animator, const EditorPlacement& placement,
          const Rig& rig, double now) {
  return eng::math::transformPoint(animator.pose(placement, rig, now)[0], {}).x;
}

}  // namespace

TEST_CASE("a prop seen for the first time plays its clip at once") {
  const Rig rig = leftRight();
  EditorPlacementAnimator animator(0.4f);
  REQUIRE(xAt(animator, prop("knight_01", "right"), rig, 0.0) == Approx(1.0f));
}

TEST_CASE("a prop whose clip changes fades to the new one") {
  const Rig rig = leftRight();
  EditorPlacementAnimator animator(0.4f);
  static_cast<void>(xAt(animator, prop("knight_01", "left"), rig, 0.0));
  // Picked in the panel at t = 1: halfway through the fade, halfway there.
  REQUIRE(xAt(animator, prop("knight_01", "right"), rig, 1.0) == Approx(-1.0f));
  REQUIRE(xAt(animator, prop("knight_01", "right"), rig, 1.2) ==
          Approx(0.0f).margin(1e-5));
  REQUIRE(xAt(animator, prop("knight_01", "right"), rig, 1.4) == Approx(1.0f));
}

TEST_CASE("each prop keeps its own playback") {
  const Rig rig = leftRight();
  EditorPlacementAnimator animator(0.4f);
  REQUIRE(xAt(animator, prop("a", "left"), rig, 0.0) == Approx(-1.0f));
  REQUIRE(xAt(animator, prop("b", "right"), rig, 0.0) == Approx(1.0f));
  REQUIRE(animator.size() == 2);
}

TEST_CASE("props not posed in a frame are forgotten at its end") {
  const Rig rig = leftRight();
  EditorPlacementAnimator animator;
  static_cast<void>(animator.pose(prop("a", "left"), rig, 0.0));
  static_cast<void>(animator.pose(prop("b", "left"), rig, 0.0));
  animator.endFrame();
  REQUIRE(animator.size() == 2);
  // Next frame "b" has been deleted, so only "a" is posed.
  static_cast<void>(animator.pose(prop("a", "left"), rig, 0.1));
  animator.endFrame();
  REQUIRE(animator.size() == 1);
}
