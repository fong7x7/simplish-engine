#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/render-mesh/skinned-mesh-posing.h>
#include <vector>

using Catch::Approx;
using eng::Mat4;
using eng::MeshData;
using eng::poseSkinnedMesh;
using eng::SkinnedMeshData;
using eng::SkinnedMeshVertex;

namespace {

/// A vertex at @p x on the X axis, facing +Z, following palette entries
/// 0 and 1 with weights @p w0 and `1 - w0`.
SkinnedMeshVertex vertexAt(float x, float w0) {
  SkinnedMeshVertex v;
  v.position = {x, 0.0f, 0.0f};
  v.normal = {0.0f, 0.0f, 1.0f};
  v.uv = {x, 0.5f};
  v.joints[0] = 0;
  v.joints[1] = 1;
  v.weights[0] = w0;
  v.weights[1] = 1.0f - w0;
  return v;
}

/// One triangle: a vertex wholly on joint 0, one wholly on joint 1, and
/// one split evenly between them.
SkinnedMeshData triangle() {
  SkinnedMeshData mesh;
  mesh.vertices = {vertexAt(0.0f, 1.0f), vertexAt(1.0f, 0.0f),
                   vertexAt(2.0f, 0.5f)};
  mesh.indices = {0, 1, 2};
  mesh.texture_path = "skin.png";
  return mesh;
}

/// A skin whose joint 1 lifts its vertices four units up Z.
std::vector<Mat4> liftJointOne() {
  std::vector<Mat4> skin{Mat4::identity(), Mat4::identity()};
  skin[1](2, 3) = 4.0f;
  return skin;
}

}  // namespace

TEST_CASE("posing with no skin gives the bind pose") {
  const MeshData posed = poseSkinnedMesh(triangle(), {});
  REQUIRE(posed.vertices.size() == 3);
  REQUIRE(posed.vertices[1].position.x == 1.0f);
  REQUIRE(posed.max.x == 2.0f);
  REQUIRE(posed.max.z == 0.0f);
}

TEST_CASE("each vertex moves by the joints it is weighted to") {
  const MeshData posed = poseSkinnedMesh(triangle(), liftJointOne());
  REQUIRE(posed.vertices[0].position.z == 0.0f);
  REQUIRE(posed.vertices[1].position.z == Approx(4.0f));
  // Half on each joint: halfway between staying and lifting.
  REQUIRE(posed.vertices[2].position.z == Approx(2.0f));
}

TEST_CASE("posed bounds are the posed mesh's") {
  const MeshData posed = poseSkinnedMesh(triangle(), liftJointOne());
  REQUIRE(posed.min.z == 0.0f);
  REQUIRE(posed.max.z == Approx(4.0f));
}

TEST_CASE("normals turn with the joints and stay unit length") {
  std::vector<Mat4> skin{Mat4::identity(), Mat4::identity()};
  // Joint 1 is a quarter turn about X, which takes +Z to -Y.
  skin[1](1, 1) = 0.0f;
  skin[1](1, 2) = -1.0f;
  skin[1](2, 1) = 1.0f;
  skin[1](2, 2) = 0.0f;
  const MeshData posed = poseSkinnedMesh(triangle(), skin);
  REQUIRE(posed.vertices[1].normal.y == Approx(-1.0f));
  REQUIRE(eng::Vec3::length(posed.vertices[2].normal) == Approx(1.0f));
}

TEST_CASE("posing keeps indices, texture coordinates and the texture") {
  const MeshData posed = poseSkinnedMesh(triangle(), liftJointOne());
  REQUIRE(posed.indices == std::vector<uint32_t>{0, 1, 2});
  REQUIRE(posed.vertices[2].uv.x == 2.0f);
  REQUIRE(posed.texture_path == "skin.png");
}

TEST_CASE("a vertex naming a missing palette entry holds that influence") {
  const std::vector<Mat4> skin{Mat4::identity()};
  const MeshData posed = poseSkinnedMesh(triangle(), skin);
  REQUIRE(posed.vertices[1].position.z == 0.0f);
}
