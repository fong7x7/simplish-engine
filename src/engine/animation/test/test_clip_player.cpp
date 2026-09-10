#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/animation/clip-player.h>
#include <engine/math/math.h>

using Catch::Approx;
using eng::Mat4;
using eng::animation::AnimationChannel;
using eng::animation::AnimationClip;
using eng::animation::ChannelTarget;
using eng::animation::ClipPlayer;
using eng::animation::Rig;
using eng::animation::RIG_REST_POSE;
using eng::animation::SKELETON_NO_PARENT;

namespace {

/// Clip numbers in `slider()`.
constexpr size_t LEFT = 0;
constexpr size_t RIGHT = 1;
constexpr size_t RAMP = 2;

/// A clip one second long moving the one joint along X from @p x0 to @p x1.
AnimationClip slide(const char* name, float x0, float x1) {
  AnimationChannel c;
  c.target = ChannelTarget::TRANSLATION;
  c.times = {0.0f, 1.0f};
  c.values = {x0, 0.0f, 0.0f, x1, 0.0f, 0.0f};
  return {name, 1.0f, {c}};
}

/// One joint at the origin, bound there, with three clips: held at -1, held
/// at +1, and sliding from 0 to 10 over its second.
Rig slider() {
  Rig rig;
  rig.skeleton.parents = {SKELETON_NO_PARENT};
  rig.skeleton.rest.resize(1);
  rig.skeleton.names = {"joint"};
  rig.skin.joints = {0};
  rig.skin.inverse_bind = {Mat4::identity()};
  rig.clips = {slide("left", -1.0f, -1.0f), slide("right", 1.0f, 1.0f),
               slide("ramp", 0.0f, 10.0f)};
  return rig;
}

/// Where the joint is at @p now: how far along X it carries the mesh.
float xAt(ClipPlayer& player, const Rig& rig, double now) {
  return eng::math::transformPoint(player.evaluate(rig, now)[0], {}).x;
}

}  // namespace

TEST_CASE("the first clip a player is given cuts in") {
  const Rig rig = slider();
  ClipPlayer player;
  player.play(RIGHT, 0.0, 0.2f);
  REQUIRE(xAt(player, rig, 0.0) == Approx(1.0f));
  REQUIRE(player.fadeWeight(0.0) == 1.0f);
}

TEST_CASE("switching clips fades from one to the other, eased") {
  const Rig rig = slider();
  ClipPlayer player;
  player.play(LEFT, 0.0, 0.0f);
  REQUIRE(xAt(player, rig, 0.0) == Approx(-1.0f));

  player.play(RIGHT, 1.0, 0.2f);
  REQUIRE(player.clip() == RIGHT);
  // At the switch, nothing has changed yet: switching never pops.
  REQUIRE(xAt(player, rig, 1.0) == Approx(-1.0f));
  // Halfway, eased or not, is half.
  REQUIRE(xAt(player, rig, 1.1) == Approx(0.0f).margin(1e-5));
  // A quarter of the way in time is less than a quarter of the way in
  // weight: the fade starts gently.
  REQUIRE(player.fadeWeight(1.05) < 0.25f);
  REQUIRE(xAt(player, rig, 1.2) == Approx(1.0f));
  REQUIRE(player.fadeWeight(1.2) == 1.0f);
}

TEST_CASE("a fade of no length cuts") {
  const Rig rig = slider();
  ClipPlayer player;
  player.play(LEFT, 0.0, 0.0f);
  static_cast<void>(player.evaluate(rig, 0.0));
  player.play(RIGHT, 1.0, 0.0f);
  REQUIRE(xAt(player, rig, 1.0) == Approx(1.0f));
}

TEST_CASE("playing the clip already playing does not restart it") {
  const Rig rig = slider();
  ClipPlayer player;
  player.play(RAMP, 0.0, 0.0f);
  REQUIRE(xAt(player, rig, 0.5) == Approx(5.0f));
  player.play(RAMP, 0.5, 0.2f);
  REQUIRE(xAt(player, rig, 0.6) == Approx(6.0f));
}

TEST_CASE("the clip fading out keeps playing while it fades") {
  const Rig rig = slider();
  ClipPlayer player;
  player.play(RAMP, 0.0, 0.0f);
  static_cast<void>(player.evaluate(rig, 0.5));
  player.play(LEFT, 0.5, 0.4f);
  // Halfway through the fade the ramp has moved on to 7, not stopped at 5:
  // halfway between 7 and -1 is 3.
  REQUIRE(xAt(player, rig, 0.7) == Approx(3.0f));
}

TEST_CASE("a switch mid-fade fades from the pose on screen, without a pop") {
  const Rig rig = slider();
  ClipPlayer player;
  player.play(LEFT, 0.0, 0.0f);
  static_cast<void>(player.evaluate(rig, 0.0));
  player.play(RIGHT, 1.0, 0.4f);
  REQUIRE(xAt(player, rig, 1.2) == Approx(0.0f).margin(1e-5));

  // Interrupted halfway: the screen shows 0, a mix of two clips, and the
  // new fade starts from exactly that.
  player.play(RAMP, 1.2, 0.4f);
  REQUIRE(xAt(player, rig, 1.2) == Approx(0.0f).margin(1e-5));
  // Halfway through the new fade: halfway from the frozen 0 to the ramp's 2.
  REQUIRE(xAt(player, rig, 1.4) == Approx(1.0f));
  REQUIRE(xAt(player, rig, 1.6) == Approx(4.0f));
}

TEST_CASE("fading to the rest pose puts the joint back where it was bound") {
  const Rig rig = slider();
  ClipPlayer player;
  player.play(RIGHT, 0.0, 0.0f);
  static_cast<void>(player.evaluate(rig, 0.0));
  player.play(RIG_REST_POSE, 0.0, 0.2f);
  REQUIRE(xAt(player, rig, 0.1) == Approx(0.5f));
  REQUIRE(xAt(player, rig, 0.2) == Approx(0.0f).margin(1e-6));
}
