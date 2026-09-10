#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <engine/animation/pose-sampling.h>
#include <engine/math/math.h>
#include <numbers>
#include <vector>

using Catch::Approx;
using eng::Mat4;
using eng::Vec3;
using eng::animation::AnimationChannel;
using eng::animation::AnimationClip;
using eng::animation::ChannelInterpolation;
using eng::animation::ChannelTarget;
using eng::animation::computeJointWorlds;
using eng::animation::computeSkinMatrices;
using eng::animation::JointPose;
using eng::animation::loopClipTime;
using eng::animation::samplePose;
using eng::animation::Skeleton;
using eng::animation::SKELETON_NO_PARENT;
using eng::animation::Skin;
using Floats = std::array<float, 4>;

namespace {

/// Pi, as the float every angle here is measured against.
constexpr float PI = std::numbers::pi_v<float>;

/// Half of the square root of two: the sine and cosine of an eighth turn.
const float HALF_SQRT2 = std::sqrt(0.5f);

/// A translation channel on joint 0 moving X from 0 to 10 between t = 1
/// and t = 3.
AnimationChannel slide(ChannelInterpolation interpolation) {
  AnimationChannel c;
  c.target = ChannelTarget::TRANSLATION;
  c.interpolation = interpolation;
  c.times = {1.0f, 3.0f};
  c.values = {0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f};
  return c;
}

/// A rotation channel from no turn to @p end_angle radians about Z over one
/// second.
AnimationChannel turnAboutZ(float end_angle) {
  AnimationChannel c;
  c.target = ChannelTarget::ROTATION;
  c.times = {0.0f, 1.0f};
  c.values = {0.0f,
              0.0f,
              0.0f,
              1.0f,
              0.0f,
              0.0f,
              std::sin(end_angle / 2.0f),
              std::cos(end_angle / 2.0f)};
  return c;
}

/// Sample @p channel at @p t into a fresh array.
Floats sampled(const AnimationChannel& channel, float t) {
  Floats out{-1.0f, -1.0f, -1.0f, -1.0f};
  eng::animation::sampleChannel(channel, t, out);
  return out;
}

/// A root at the origin and a child one unit up +Y from it.
Skeleton twoJoints() {
  Skeleton s;
  s.parents = {SKELETON_NO_PARENT, 0};
  s.rest.resize(2);
  s.rest[1].translation = {0.0f, 1.0f, 0.0f};
  s.names = {"root", "tip"};
  return s;
}

}  // namespace

TEST_CASE("looping playback wraps into the clip") {
  AnimationClip clip;
  clip.duration = 2.0f;
  REQUIRE(loopClipTime(clip, 0.5) == Approx(0.5f));
  REQUIRE(loopClipTime(clip, 4.5) == Approx(0.5f));
  REQUIRE(loopClipTime(clip, -0.5) == Approx(1.5f));
}

TEST_CASE("a clip with no length is always at its start") {
  REQUIRE(loopClipTime(AnimationClip{}, 3.0) == 0.0f);
}

TEST_CASE("a channel holds its end keys outside its time range") {
  const AnimationChannel c = slide(ChannelInterpolation::LINEAR);
  REQUIRE(sampled(c, 0.0f)[0] == 0.0f);
  REQUIRE(sampled(c, 5.0f)[0] == 10.0f);
}

TEST_CASE("a linear channel moves in a straight line between keys") {
  const AnimationChannel c = slide(ChannelInterpolation::LINEAR);
  REQUIRE(sampled(c, 1.5f)[0] == Approx(2.5f));
  REQUIRE(sampled(c, 2.0f)[0] == Approx(5.0f));
}

TEST_CASE("a translation channel writes three floats and no more") {
  REQUIRE(sampled(slide(ChannelInterpolation::LINEAR), 2.0f)[3] == -1.0f);
}

TEST_CASE("a step channel holds each key until the next") {
  const AnimationChannel c = slide(ChannelInterpolation::STEP);
  REQUIRE(sampled(c, 2.9f)[0] == 0.0f);
  REQUIRE(sampled(c, 3.0f)[0] == 10.0f);
}

TEST_CASE("a channel with no keys leaves the value alone") {
  REQUIRE(sampled(AnimationChannel{}, 1.0f)[0] == -1.0f);
}

TEST_CASE("a channel whose values run short is ignored") {
  AnimationChannel c = slide(ChannelInterpolation::LINEAR);
  c.values.pop_back();
  REQUIRE(sampled(c, 2.0f)[0] == -1.0f);
}

TEST_CASE("a rotation blends at constant angular speed") {
  // A quarter turn, a quarter of the way: an eighth of a quarter — pi/8 —
  // which a straight blend of the quaternions would not reach exactly.
  const Floats q = sampled(turnAboutZ(PI / 2.0f), 0.25f);
  REQUIRE(q[2] == Approx(std::sin(PI / 16.0f)));
  REQUIRE(q[3] == Approx(std::cos(PI / 16.0f)));
}

TEST_CASE("a rotation takes the shorter way round") {
  // The end key is the quarter turn written as its negation; same rotation,
  // opposite hemisphere. Halfway must be an eighth turn, not most of one.
  AnimationChannel c = turnAboutZ(PI / 2.0f);
  for (size_t i = 4; i < 8; ++i) {
    c.values[i] = -c.values[i];
  }
  const Floats q = sampled(c, 0.5f);
  const float w = std::abs(q[3]);
  REQUIRE(w == Approx(std::cos(PI / 8.0f)));
}

TEST_CASE("a cubic spline eases through keys with flat tangents") {
  AnimationChannel c = slide(ChannelInterpolation::CUBIC_SPLINE);
  // In-tangent, value, out-tangent per key; all tangents zero.
  c.values = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0};
  REQUIRE(sampled(c, 2.0f)[0] == Approx(5.0f));
  // A quarter of the way along, an ease-in is well short of a quarter.
  REQUIRE(sampled(c, 1.5f)[0] == Approx(1.5625f));
}

TEST_CASE("a cubic spline with matching tangents is a straight line") {
  AnimationChannel c = slide(ChannelInterpolation::CUBIC_SPLINE);
  // Ten units over two seconds is five per second, in and out of each key.
  c.values = {5, 0, 0, 0, 0, 0, 5, 0, 0, 5, 0, 0, 10, 0, 0, 5, 0, 0};
  REQUIRE(sampled(c, 1.5f)[0] == Approx(2.5f));
}

TEST_CASE("sampling a pose keeps the rest pose where no channel reaches") {
  AnimationClip clip;
  clip.channels.push_back(slide(ChannelInterpolation::LINEAR));
  std::vector<JointPose> pose(2);
  samplePose(twoJoints(), clip, 2.0f, pose);
  REQUIRE(pose[0].translation.x == Approx(5.0f));
  REQUIRE(pose[1].translation.y == 1.0f);
}

TEST_CASE("a channel naming a joint the skeleton lacks is skipped") {
  AnimationClip clip;
  clip.channels.push_back(slide(ChannelInterpolation::LINEAR));
  clip.channels.back().joint = 7;
  std::vector<JointPose> pose(2);
  samplePose(twoJoints(), clip, 2.0f, pose);
  REQUIRE(pose[0].translation.x == 0.0f);
}

TEST_CASE("a child's world transform carries its parent's") {
  const Skeleton s = twoJoints();
  std::vector<JointPose> pose = s.rest;
  pose[0].rotation = {0.0f, 0.0f, HALF_SQRT2, HALF_SQRT2};
  std::vector<Mat4> worlds(2);
  computeJointWorlds(s, pose, worlds);
  // The tip sits one up +Y; its parent's quarter turn swings it to -X.
  const Vec3 tip = eng::math::transformPoint(worlds[1], {});
  REQUIRE(tip.x == Approx(-1.0f));
  REQUIRE(tip.y == Approx(0.0f).margin(1e-6));
}

TEST_CASE("in the bind pose every skin matrix is the identity") {
  const Skeleton s = twoJoints();
  Skin skin;
  skin.joints = {0, 1};
  skin.inverse_bind = {Mat4::identity(), Mat4::identity()};
  skin.inverse_bind[1](1, 3) = -1.0f;
  std::vector<Mat4> worlds(2);
  computeJointWorlds(s, s.rest, worlds);
  std::vector<Mat4> out(2);
  computeSkinMatrices(skin, worlds, out);
  for (size_t i = 0; i < 16; ++i) {
    REQUIRE(out[1][i] == Approx(Mat4::identity()[i]).margin(1e-6));
  }
}

TEST_CASE("a skin's root is applied above every joint") {
  Skin skin;
  skin.joints = {0};
  skin.inverse_bind = {Mat4::identity()};
  skin.root(2, 3) = 4.0f;
  const std::vector<Mat4> worlds{Mat4::identity()};
  std::vector<Mat4> out(1);
  computeSkinMatrices(skin, worlds, out);
  REQUIRE(eng::math::transformPoint(out[0], {}).z == Approx(4.0f));
}

TEST_CASE("a skin joint naming a missing skeleton joint stays put") {
  Skin skin;
  skin.joints = {3};
  skin.inverse_bind = {Mat4::identity()};
  skin.root(0, 3) = 9.0f;
  const std::vector<Mat4> worlds{Mat4::identity()};
  std::vector<Mat4> out(1);
  computeSkinMatrices(skin, worlds, out);
  REQUIRE(eng::math::transformPoint(out[0], {}).x == 0.0f);
}

TEST_CASE("blending poses moves each part of each joint between them") {
  std::vector<JointPose> from(1);
  std::vector<JointPose> to(1);
  from[0].translation = {0.0f, 2.0f, 0.0f};
  to[0].translation = {4.0f, 2.0f, 0.0f};
  to[0].scale = {3.0f, 1.0f, 1.0f};
  to[0].rotation = {0.0f, 0.0f, HALF_SQRT2, HALF_SQRT2};
  std::vector<JointPose> out(1);
  eng::animation::blendPoses(from, to, 0.5f, out);
  REQUIRE(out[0].translation.x == Approx(2.0f));
  REQUIRE(out[0].scale.x == Approx(2.0f));
  // Half a quarter turn about Z.
  REQUIRE(out[0].rotation.z == Approx(std::sin(PI / 8.0f)));
  REQUIRE(out[0].rotation.w == Approx(std::cos(PI / 8.0f)));
}

TEST_CASE("blend weights past either end are clamped") {
  std::vector<JointPose> from(1);
  std::vector<JointPose> to(1);
  to[0].translation.x = 1.0f;
  std::vector<JointPose> out(1);
  eng::animation::blendPoses(from, to, 2.0f, out);
  REQUIRE(out[0].translation.x == 1.0f);
  eng::animation::blendPoses(from, to, -1.0f, out);
  REQUIRE(out[0].translation.x == 0.0f);
}

TEST_CASE("a blend may be written over one of its inputs") {
  std::vector<JointPose> from(1);
  std::vector<JointPose> to(1);
  to[0].translation.x = 8.0f;
  eng::animation::blendPoses(from, to, 0.25f, to);
  REQUIRE(to[0].translation.x == Approx(2.0f));
}
