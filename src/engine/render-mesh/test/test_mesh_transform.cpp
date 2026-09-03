#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/render-mesh/mesh-transform.h>

using Catch::Approx;
using namespace eng;

namespace {

/// A mesh standing one unit tall along Y, the way an exporter writes it.
MeshData yUpColumn() {
  MeshData mesh;
  mesh.vertices.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}});
  mesh.vertices.push_back({{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}});
  mesh.vertices.push_back({{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}});
  mesh.indices = {0, 1, 2};
  mesh.min = {0.0f, 0.0f, 0.0f};
  mesh.max = {1.0f, 1.0f, 0.0f};
  return mesh;
}

}  // namespace

TEST_CASE("height moves from Y to Z") {
  MeshData mesh = yUpColumn();
  orientYUpToZUp(mesh);
  // The vertex that was one unit up in Y is now one unit up in Z; without
  // this the model lies on its side in the viewport.
  REQUIRE(mesh.vertices[1].position.z == Approx(1.0f));
  REQUIRE(mesh.vertices[1].position.y == Approx(0.0f));
}

TEST_CASE("normals rotate with the geometry") {
  MeshData mesh = yUpColumn();
  orientYUpToZUp(mesh);
  // An up-facing normal has to keep facing up, or the shading is wrong in a
  // way no transform later can fix.
  for (const auto& vertex : mesh.vertices) {
    REQUIRE(vertex.normal.z == Approx(1.0f));
  }
}

TEST_CASE("the X axis is untouched") {
  MeshData mesh = yUpColumn();
  orientYUpToZUp(mesh);
  REQUIRE(mesh.vertices[2].position.x == Approx(1.0f));
}

TEST_CASE("bounds follow the rotation") {
  MeshData mesh = yUpColumn();
  orientYUpToZUp(mesh);
  REQUIRE(mesh.max.z == Approx(1.0f));
  REQUIRE(mesh.max.y == Approx(0.0f));
}

TEST_CASE("an empty mesh is left alone") {
  MeshData mesh;
  orientYUpToZUp(mesh);
  REQUIRE(mesh.vertices.empty());
}
