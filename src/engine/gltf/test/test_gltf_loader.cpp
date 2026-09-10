#include "support/test-gltf.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <engine/animation/rig-pose.h>
#include <engine/gltf/gltf-loader.h>
#include <engine/render-mesh/skinned-mesh-posing.h>
#include <filesystem>
#include <fstream>

using Catch::Approx;
using eng::gltf::GltfLoadError;
using eng::gltf::loadGltfModel;
using eng::gltf::parseGlbModel;
using eng::gltf::parseGltfModel;
using eng::gltf::SkinnedModel;
using eng::gltf::test::armDocument;
using eng::gltf::test::TestGltf;

namespace {

/// @p g parsed as `.gltf` text with no directory to resolve against.
std::expected<SkinnedModel, GltfLoadError> parse(const TestGltf& g) {
  return parseGltfModel(g.gltfText(), {}, {});
}

/// The error @p g fails with, or nothing when it loads.
std::optional<GltfLoadError> failureOf(const TestGltf& g) {
  const auto model = parse(g);
  return model ? std::nullopt : std::optional(model.error());
}

/// The arm's mesh posed @p seconds into clip @p clip.
eng::MeshData posedArm(const SkinnedModel& model, size_t clip, float seconds) {
  eng::animation::RigPose pose;
  return eng::poseSkinnedMesh(model.mesh,
                              pose.evaluate(model.rig, clip, seconds));
}

/// A fresh, empty directory under the system's temporary one.
std::filesystem::path scratchDir(const char* name) {
  const auto dir = std::filesystem::temp_directory_path() / name;
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

/// Point the arm's triangle at a base colour map named @p image.
void useBaseColorImage(nlohmann::json& doc, const char* image) {
  doc["images"] = {{{"uri", image}}};
  doc["textures"] = {{{"source", 0}}};
  doc["materials"] = {
      {{"pbrMetallicRoughness", {{"baseColorTexture", {{"index", 0}}}}}}};
  doc["meshes"][0]["primitives"][0]["material"] = 0;
}

/// Write @p g into @p dir as `arm.gltf`, with its buffer beside it as
/// `arm data.bin` — named by an escaped URI, as exporters write spaces —
/// and a base colour map `skin.png`.
void writeArmFiles(const TestGltf& g, const std::filesystem::path& dir) {
  nlohmann::json doc = nlohmann::json::parse(g.gltfText());
  doc["buffers"][0]["uri"] = "arm%20data.bin";
  useBaseColorImage(doc, "skin.png");
  std::ofstream(dir / "arm data.bin", std::ios::binary)
      .write(reinterpret_cast<const char*>(g.bin().data()),
             static_cast<std::streamsize>(g.bin().size()));
  std::ofstream(dir / "skin.png") << "png";
  std::ofstream(dir / "arm.gltf") << doc.dump();
}

}  // namespace

TEST_CASE("the arm's skeleton is reordered parents first") {
  const auto model = parse(armDocument());
  REQUIRE(model.has_value());
  const auto& skeleton = model->rig.skeleton;
  REQUIRE(skeleton.names ==
          std::vector<std::string>{"Armature", "shoulder", "elbow"});
  REQUIRE(skeleton.parents[0] == eng::animation::SKELETON_NO_PARENT);
  REQUIRE(skeleton.parents[1] == 0);
  REQUIRE(skeleton.parents[2] == 1);
  REQUIRE(skeleton.rest[2].translation.y == 1.0f);
  REQUIRE(eng::animation::skeletonIsOrdered(skeleton));
}

TEST_CASE("the skin's joints are renumbered into the skeleton") {
  const auto model = parse(armDocument());
  REQUIRE(model.has_value());
  REQUIRE(model->rig.skin.joints == std::vector<uint32_t>{1, 2});
  REQUIRE(model->rig.skin.inverse_bind[1](1, 3) == -1.0f);
}

TEST_CASE("the arm's triangle keeps its weights, scaled to one") {
  const auto model = parse(armDocument());
  REQUIRE(model.has_value());
  const auto& mesh = model->mesh;
  REQUIRE(mesh.vertices.size() == 3);
  REQUIRE(mesh.indices == std::vector<uint32_t>{0, 1, 2});
  REQUIRE(mesh.vertices[1].joints[0] == 1);
  REQUIRE(mesh.vertices[1].weights[0] == 1.0f);
  REQUIRE(mesh.max.y == 2.0f);
  REQUIRE(mesh.max.x == 1.0f);
}

TEST_CASE("the arm's clip drives the shoulder joint") {
  const auto model = parse(armDocument());
  REQUIRE(model.has_value());
  REQUIRE(model->rig.clips.size() == 1);
  const auto& clip = model->rig.clips[0];
  REQUIRE(clip.name == "swing");
  REQUIRE(clip.duration == 1.0f);
  REQUIRE(clip.channels.size() == 1);
  REQUIRE(clip.channels[0].joint == 1);
}

TEST_CASE("playing the loaded clip swings the arm") {
  const auto model = parse(armDocument());
  REQUIRE(model.has_value());
  const eng::MeshData rest =
      posedArm(*model, eng::animation::RIG_REST_POSE, 0.0f);
  REQUIRE(rest.vertices[2].position.x == Approx(1.0f));
  REQUIRE(rest.vertices[2].position.y == Approx(2.0f));
  // A quarter turn about Z at the shoulder takes (1, 2) to (-2, 1).
  const eng::MeshData swung = posedArm(*model, 0, 1.0f);
  REQUIRE(swung.vertices[2].position.x == Approx(-2.0f));
  REQUIRE(swung.vertices[2].position.y == Approx(1.0f));
  REQUIRE(swung.vertices[0].position.x == Approx(0.0f).margin(1e-6));
}

TEST_CASE("the same arm loads from a GLB container") {
  const auto model = parseGlbModel(armDocument().glbBytes(), {});
  REQUIRE(model.has_value());
  REQUIRE(model->mesh.vertices.size() == 3);
  REQUIRE(model->rig.clips.size() == 1);
}

TEST_CASE("a node above the skeleton moves every joint") {
  TestGltf g = armDocument();
  g.doc()["nodes"][2]["scale"] = {2, 2, 2};
  const auto model = parse(g);
  REQUIRE(model.has_value());
  const eng::MeshData rest =
      posedArm(*model, eng::animation::RIG_REST_POSE, 0.0f);
  REQUIRE(rest.vertices[1].position.y == Approx(4.0f));
}

TEST_CASE("a primitive without normals gets smooth ones") {
  TestGltf g = armDocument();
  g.doc()["meshes"][0]["primitives"][0]["attributes"].erase("NORMAL");
  const auto model = parse(g);
  REQUIRE(model.has_value());
  const eng::Vec3 n = model->mesh.vertices[0].normal;
  REQUIRE(std::abs(n.z) == Approx(1.0f));
}

TEST_CASE("channels on nodes outside the skeleton are dropped") {
  TestGltf g = armDocument();
  g.doc()["animations"][0]["channels"][0]["target"]["node"] = 3;
  const auto model = parse(g);
  REQUIRE(model.has_value());
  REQUIRE(model->rig.clips[0].channels.empty());
}

TEST_CASE("an unnamed animation is named for its place") {
  TestGltf g = armDocument();
  g.doc()["animations"][0].erase("name");
  const auto model = parse(g);
  REQUIRE(model.has_value());
  REQUIRE(model->rig.clips[0].name == "animation 0");
}

TEST_CASE("text that is not JSON is malformed") {
  REQUIRE(parseGltfModel("{nope", {}, {}).error() == GltfLoadError::MALFORMED);
  REQUIRE(parseGlbModel(std::vector<uint8_t>{1, 2, 3}, {}).error() ==
          GltfLoadError::MALFORMED);
}

TEST_CASE("a glTF 1.0 file is refused") {
  TestGltf g = armDocument();
  g.doc()["asset"]["version"] = "1.0";
  REQUIRE(failureOf(g) == GltfLoadError::UNSUPPORTED_VERSION);
}

TEST_CASE("a required extension is refused unless it is quantization") {
  TestGltf g = armDocument();
  g.doc()["extensionsRequired"] = {"KHR_mesh_quantization"};
  REQUIRE_FALSE(failureOf(g).has_value());
  g.doc()["extensionsRequired"] = {"KHR_draco_mesh_compression"};
  REQUIRE(failureOf(g) == GltfLoadError::UNSUPPORTED_EXTENSION);
}

TEST_CASE("a model with no skin is not a skinned model") {
  TestGltf g = armDocument();
  g.doc()["nodes"][3].erase("skin");
  REQUIRE(failureOf(g) == GltfLoadError::NO_SKINNED_MESH);
}

TEST_CASE("a vertex weighted to a joint the skin lacks is refused") {
  TestGltf g = armDocument();
  const size_t joints =
      g.addBytes({0, 0, 0, 0, 7, 0, 0, 0, 1, 0, 0, 0}, "VEC4");
  g.doc()["meshes"][0]["primitives"][0]["attributes"]["JOINTS_0"] = joints;
  REQUIRE(failureOf(g) == GltfLoadError::BAD_SKIN);
}

TEST_CASE("a skin with more joints than a draw carries is refused") {
  TestGltf g = armDocument();
  auto joints = nlohmann::json::array();
  for (int i = 0; i < 81; ++i) {
    g.doc()["nodes"].push_back({{"name", "bone"}});
    joints.push_back(4 + i);
  }
  g.doc()["skins"][0]["joints"] = joints;
  g.doc()["skins"][0].erase("inverseBindMatrices");
  REQUIRE(failureOf(g) == GltfLoadError::TOO_MANY_JOINTS);
}

TEST_CASE("a hierarchy that loops is refused") {
  TestGltf g = armDocument();
  g.doc()["nodes"][2].erase("children");
  g.doc()["nodes"][0]["children"] = {1};  // elbow ↔ shoulder
  REQUIRE(failureOf(g) == GltfLoadError::BAD_SKIN);
}

TEST_CASE("a buffer shorter than it claims is missing") {
  TestGltf g = armDocument();
  nlohmann::json doc = nlohmann::json::parse(g.gltfText());
  doc["buffers"][0]["byteLength"] = 100000;
  REQUIRE(parseGltfModel(doc.dump(), {}, {}).error() ==
          GltfLoadError::MISSING_BUFFER);
}

TEST_CASE("an accessor that runs past its view is refused") {
  TestGltf g = armDocument();
  g.doc()["accessors"][0]["count"] = 50;
  REQUIRE(failureOf(g) == GltfLoadError::BAD_ACCESSOR);
}

TEST_CASE("a file on disk resolves its buffer and texture beside it") {
  const auto dir = scratchDir("simplish-gltf-loader-test");
  writeArmFiles(armDocument(), dir);
  const auto model = loadGltfModel(dir / "arm.gltf");
  REQUIRE(model.has_value());
  REQUIRE(model->mesh.vertices.size() == 3);
  REQUIRE(model->mesh.texture_path == dir / "skin.png");
}

TEST_CASE("a file that is not there is unreadable") {
  REQUIRE(loadGltfModel("/no/such/model.glb").error() ==
          GltfLoadError::UNREADABLE);
}
