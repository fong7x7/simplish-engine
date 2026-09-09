#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/render-mesh/obj-loader.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace eng;

namespace {

/// A temp directory holding a model and whatever it references, removed
/// when the test scope exits.
class ModelDir {
public:
  explicit ModelDir(const std::string& label) {
    path_ = fs::temp_directory_path() /
            ("simplish-obj-" + label + "-" +
             std::to_string(reinterpret_cast<uintptr_t>(this)));
    std::error_code ec;
    fs::remove_all(path_, ec);
    fs::create_directories(path_, ec);
  }
  ~ModelDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  ModelDir(const ModelDir&) = delete;
  ModelDir& operator=(const ModelDir&) = delete;
  ModelDir(ModelDir&&) = delete;
  ModelDir& operator=(ModelDir&&) = delete;

  /// Write a file under the directory and hand back its path.
  fs::path write(const std::string& name, const std::string& text) const {
    const fs::path file = path_ / name;
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary);
    out << text;
    return file;
  }

  /// A triangle naming @p library and @p material.
  fs::path writeModel(const std::string& library,
                      const std::string& material) const {
    std::string text;
    if (!library.empty()) {
      text += "mtllib " + library + "\n";
    }
    if (!material.empty()) {
      text += "usemtl " + material + "\n";
    }
    text += "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    return write("model.obj", text);
  }

private:
  fs::path path_;
};

}  // namespace

TEST_CASE("a model's diffuse map is resolved beside it") {
  const ModelDir dir("resolve");
  dir.write("crate.png", "not really a png");
  dir.write("crate.mtl", "newmtl body\nmap_Kd crate.png\n");
  const fs::path model = dir.writeModel("crate.mtl", "body");

  const auto mesh = loadObjMesh(model);
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->texture_path == model.parent_path() / "crate.png");
}

TEST_CASE("a map in a subdirectory resolves against the model") {
  const ModelDir dir("subdir");
  dir.write("textures/crate.png", "not really a png");
  dir.write("crate.mtl", "newmtl body\nmap_Kd textures/crate.png\n");
  const fs::path model = dir.writeModel("crate.mtl", "body");

  const auto mesh = loadObjMesh(model);
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->texture_path == model.parent_path() / "textures/crate.png");
}

TEST_CASE("a map the material names but disk does not have resolves to none") {
  // Better to report nothing than a path that will fail to decode later:
  // the caller has one thing to check rather than two.
  const ModelDir dir("missing-image");
  dir.write("crate.mtl", "newmtl body\nmap_Kd gone.png\n");
  const fs::path model = dir.writeModel("crate.mtl", "body");

  const auto mesh = loadObjMesh(model);
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->texture_path.empty());
}

TEST_CASE("a material library that is not there leaves the mesh untextured") {
  const ModelDir dir("missing-mtl");
  const fs::path model = dir.writeModel("gone.mtl", "body");

  const auto mesh = loadObjMesh(model);
  REQUIRE(mesh.has_value());
  // The geometry still loads, which is the point: a model with a broken
  // material reference is still a model.
  REQUIRE(mesh->indices.size() == 3);
  REQUIRE(mesh->texture_path.empty());
}

TEST_CASE("a model naming no library resolves nothing and still loads") {
  const ModelDir dir("no-library");
  const fs::path model = dir.writeModel("", "");

  const auto mesh = loadObjMesh(model);
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->texture_path.empty());
}

TEST_CASE("a single-material library resolves without a usemtl") {
  const ModelDir dir("no-usemtl");
  dir.write("only.png", "not really a png");
  dir.write("only.mtl", "newmtl only\nmap_Kd only.png\n");
  const fs::path model = dir.writeModel("only.mtl", "");

  const auto mesh = loadObjMesh(model);
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->texture_path == model.parent_path() / "only.png");
}
