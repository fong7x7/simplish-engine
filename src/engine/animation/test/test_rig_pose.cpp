#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <engine/animation/rig-pose.h>
#include <engine/math/math.h>

using Catch::Approx;
using eng::Mat4;
using eng::Vec3;
using eng::animation::AnimationChannel;
using eng::animation::ChannelTarget;
using eng::animation::Rig;
using eng::animation::RIG_REST_POSE;
using eng::animation::RigPose;
using eng::animation::SKELETON_NO_PARENT;

namespace {

/// An arm: a shoulder at the origin and an elbow one unit up +Y, bound in
/// that pose, with one clip that swings the shoulder a quarter turn about Z
/// over a second.
Rig swingingArm() {
  Rig rig;
  rig.skeleton.parents = {SKELETON_NO_PARENT, 0};
  rig.skeleton.rest.resize(2);
  rig.skeleton.rest[1].translation = {0.0f, 1.0f, 0.0f};
  rig.skeleton.names = {"shoulder", "elbow"};
  rig.skin.joints = {0, 1};
  rig.skin.inverse_bind = {Mat4::identity(), Mat4::identity()};
  rig.skin.inverse_bind[1](1, 3) = -1.0f;
  AnimationChannel swing;
  swing.target = ChannelTarget::ROTATION;
  swing.times = {0.0f, 1.0f};
  const float half = std::sqrt(0.5f);
  swing.values = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, half, half};
  rig.clips.push_back({"swing", 1.0f, {swing}});
  return rig;
}

}  // namespace

TEST_CASE("a rig at rest draws its mesh as modelled") {
  RigPose pose;
  const auto skin = pose.evaluate(swingingArm(), RIG_REST_POSE, 0.0f);
  REQUIRE(skin.size() == 2);
  // A point on the forearm, two units up, hangs from the elbow.
  const Vec3 p = eng::math::transformPoint(skin[1], {0.0f, 2.0f, 0.0f});
  REQUIRE(p.x == Approx(0.0f).margin(1e-6));
  REQUIRE(p.y == Approx(2.0f));
}

TEST_CASE("a clip carries the mesh with the joints it moves") {
  RigPose pose;
  const auto skin = pose.evaluate(swingingArm(), 0, 1.0f);
  // A full quarter turn at the shoulder swings the forearm from +Y to -X.
  const Vec3 p = eng::math::transformPoint(skin[1], {0.0f, 2.0f, 0.0f});
  REQUIRE(p.x == Approx(-2.0f));
  REQUIRE(p.y == Approx(0.0f).margin(1e-5));
}

TEST_CASE("a clip index past the rig's clips is the rest pose") {
  RigPose pose;
  const auto skin = pose.evaluate(swingingArm(), 5, 1.0f);
  const Vec3 p = eng::math::transformPoint(skin[1], {0.0f, 2.0f, 0.0f});
  REQUIRE(p.y == Approx(2.0f));
}

TEST_CASE("the joint worlds of the last pose stay readable") {
  RigPose pose;
  static_cast<void>(pose.evaluate(swingingArm(), 0, 1.0f));
  REQUIRE(pose.jointWorlds().size() == 2);
  const Vec3 elbow = eng::math::transformPoint(pose.jointWorlds()[1], {});
  REQUIRE(elbow.x == Approx(-1.0f));
}

TEST_CASE("posing a smaller rig after a bigger one shrinks the palette") {
  RigPose pose;
  static_cast<void>(pose.evaluate(swingingArm(), 0, 0.5f));
  Rig single;
  single.skeleton.parents = {SKELETON_NO_PARENT};
  single.skeleton.rest.resize(1);
  single.skeleton.names = {"only"};
  single.skin.joints = {0};
  single.skin.inverse_bind = {Mat4::identity()};
  REQUIRE(pose.evaluate(single, RIG_REST_POSE, 0.0f).size() == 1);
}
