#include "support/test-gltf.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/animation/rig-pose.h>
#include <engine/gltf/gltf-loader.h>
#include <engine/gltf/skinned-model-orientation.h>
#include <engine/render-mesh/skinned-mesh-posing.h>

using Catch::Approx;
using eng::gltf::orientSkinnedYUpToZUp;
using eng::gltf::SkinnedModel;
using eng::gltf::test::armDocument;

namespace {

/// The test arm, loaded.
SkinnedModel arm() {
  auto model = eng::gltf::parseGltfModel(armDocument().gltfText(), {}, {});
  REQUIRE(model.has_value());
  return std::move(*model);
}

/// @p model's mesh posed @p seconds into its first clip.
eng::MeshData swung(const SkinnedModel& model, float seconds) {
  eng::animation::RigPose pose;
  return eng::poseSkinnedMesh(model.mesh, pose.evaluate(model.rig, 0, seconds));
}

}  // namespace

TEST_CASE("turning a model Z-up stands its bounds up the Z axis") {
  SkinnedModel model = arm();
  orientSkinnedYUpToZUp(model);
  REQUIRE(model.mesh.max.z == 2.0f);
  REQUIRE(model.mesh.min.z == 0.0f);
  REQUIRE(model.mesh.max.y == Approx(0.0f).margin(1e-6));
  REQUIRE(model.mesh.vertices[1].position.z == 2.0f);
  REQUIRE(model.mesh.vertices[0].normal.y == -1.0f);
}

TEST_CASE("a turned model at rest is the model, turned") {
  SkinnedModel model = arm();
  orientSkinnedYUpToZUp(model);
  eng::animation::RigPose pose;
  const eng::MeshData rest = eng::poseSkinnedMesh(
      model.mesh,
      pose.evaluate(model.rig, eng::animation::RIG_REST_POSE, 0.0f));
  REQUIRE(rest.vertices[2].position.x == Approx(1.0f));
  REQUIRE(rest.vertices[2].position.z == Approx(2.0f));
}

TEST_CASE("a turned model plays its clips turned, not on its side") {
  const eng::MeshData before = swung(arm(), 1.0f);
  SkinnedModel turned = arm();
  orientSkinnedYUpToZUp(turned);
  const eng::MeshData after = swung(turned, 1.0f);
  for (size_t i = 0; i < before.vertices.size(); ++i) {
    const eng::Vec3 b = before.vertices[i].position;
    const eng::Vec3 a = after.vertices[i].position;
    REQUIRE(a.x == Approx(b.x).margin(1e-5));
    REQUIRE(a.y == Approx(-b.z).margin(1e-5));
    REQUIRE(a.z == Approx(b.y).margin(1e-5));
  }
}
