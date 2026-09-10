#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <engine/animation/joint-pose.h>
#include <engine/math/math.h>

using Catch::Approx;
using eng::Mat4;
using eng::Quat;
using eng::Vec3;
using eng::animation::JointPose;
using eng::animation::jointPoseMatrix;

namespace {

/// A quarter turn about +Z, which takes +X to +Y.
Quat quarterTurnAboutZ() {
  const float half = std::sqrt(0.5f);
  return {0.0f, 0.0f, half, half};
}

}  // namespace

TEST_CASE("a default joint pose is the identity") {
  const Mat4 m = jointPoseMatrix(JointPose{});
  const Mat4 identity = Mat4::identity();
  for (size_t i = 0; i < 16; ++i) {
    REQUIRE(m[i] == Approx(identity[i]).margin(1e-6));
  }
}

TEST_CASE("a joint pose scales, then rotates, then translates") {
  JointPose pose;
  pose.translation = {10.0f, 0.0f, 0.0f};
  pose.rotation = quarterTurnAboutZ();
  pose.scale = {2.0f, 3.0f, 1.0f};
  // +X scaled by 2 is (2, 0, 0); turned a quarter about Z it is (0, 2, 0);
  // moved, (10, 2, 0). Any other order lands somewhere else.
  const Vec3 x = eng::math::transformPoint(jointPoseMatrix(pose), {1, 0, 0});
  REQUIRE(x.x == Approx(10.0f));
  REQUIRE(x.y == Approx(2.0f));
  REQUIRE(x.z == Approx(0.0f).margin(1e-6));
  // +Y scaled by 3 is (0, 3, 0); turned, (-3, 0, 0).
  const Vec3 y = eng::math::transformPoint(jointPoseMatrix(pose), {0, 1, 0});
  REQUIRE(y.x == Approx(7.0f));
  REQUIRE(y.y == Approx(0.0f).margin(1e-6));
}
