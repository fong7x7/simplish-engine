#include "gltf-node-pose.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using Catch::Approx;
using eng::Mat4;
using eng::animation::JointPose;
using eng::animation::jointPoseMatrix;
using eng::gltf::decomposeJointMatrix;
using eng::gltf::gltfNodePose;
using eng::gltf::Json;

namespace {

/// Whether two matrices agree to within rounding.
bool sameMatrix(const Mat4& a, const Mat4& b) {
  for (size_t i = 0; i < 16; ++i) {
    if (std::abs(a[i] - b[i]) > 1e-5f) {
      return false;
    }
  }
  return true;
}

/// A pose with every part away from the identity, turned about an axis
/// that is none of X, Y, and Z.
JointPose skewedPose() {
  JointPose pose;
  pose.translation = {1.0f, -2.0f, 3.0f};
  pose.rotation = eng::Quat::normalize({0.3f, -0.5f, 0.1f, 0.8f});
  pose.scale = {2.0f, 0.5f, 1.5f};
  return pose;
}

}  // namespace

TEST_CASE("a node with no transform is at rest at the identity") {
  const JointPose pose = gltfNodePose(Json::object());
  REQUIRE(sameMatrix(jointPoseMatrix(pose), Mat4::identity()));
}

TEST_CASE("a node's translation, rotation and scale are read") {
  const Json node = {{"translation", {1, 2, 3}},
                     {"rotation", {0, 0, 1, 0}},
                     {"scale", {4, 5, 6}}};
  const JointPose pose = gltfNodePose(node);
  REQUIRE(pose.translation.z == 3.0f);
  REQUIRE(pose.rotation.z == 1.0f);
  REQUIRE(pose.rotation.w == 0.0f);
  REQUIRE(pose.scale.y == 5.0f);
}

TEST_CASE("a matrix taken apart builds the same matrix again") {
  const Mat4 m = jointPoseMatrix(skewedPose());
  const JointPose back = decomposeJointMatrix(m);
  REQUIRE(back.scale.x == Approx(2.0f));
  REQUIRE(back.translation.y == Approx(-2.0f));
  REQUIRE(sameMatrix(jointPoseMatrix(back), m));
}

TEST_CASE("each branch of the quaternion recovery is exact") {
  // Half turns about X, Y, and Z have a zero trace's worth of w, which is
  // what sends the recovery down each of its other three branches.
  for (const eng::Quat q :
       {eng::Quat{1, 0, 0, 0}, eng::Quat{0, 1, 0, 0}, eng::Quat{0, 0, 1, 0}}) {
    JointPose pose;
    pose.rotation = q;
    const Mat4 m = jointPoseMatrix(pose);
    REQUIRE(sameMatrix(jointPoseMatrix(decomposeJointMatrix(m)), m));
  }
}

TEST_CASE("a node's matrix is taken apart into its parts") {
  const Mat4 m = jointPoseMatrix(skewedPose());
  Json node;
  node["matrix"] = Json::array();
  for (size_t i = 0; i < 16; ++i) {
    node["matrix"].push_back(m[i]);
  }
  REQUIRE(sameMatrix(jointPoseMatrix(gltfNodePose(node)), m));
}

TEST_CASE("a mirroring matrix puts its negative scale on X") {
  Mat4 m = Mat4::identity();
  m(1, 1) = -1.0f;
  const JointPose pose = decomposeJointMatrix(m);
  REQUIRE(pose.scale.x == Approx(-1.0f));
  REQUIRE(sameMatrix(jointPoseMatrix(pose), m));
}
