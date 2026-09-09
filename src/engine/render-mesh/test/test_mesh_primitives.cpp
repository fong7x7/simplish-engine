#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <engine/render-mesh/mesh-primitives.h>
#include <vector>

using Catch::Approx;
using namespace eng;

namespace {

/// Every primitive, so the shared promises are asserted over all of them
/// rather than remembered for one.
std::vector<MeshData> allPrimitives() {
  return {makeCubeMesh(), makeSphereMesh(), makePyramidMesh(),
          makeCylinderMesh()};
}

/// Whether a mesh's indices all name a vertex it has.
bool indicesAreInRange(const MeshData& mesh) {
  for (const uint32_t index : mesh.indices) {
    if (index >= mesh.vertices.size()) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST_CASE("every primitive is a whole triangle list over its own vertices") {
  for (const MeshData& mesh : allPrimitives()) {
    REQUIRE_FALSE(mesh.vertices.empty());
    REQUIRE(mesh.indices.size() % 3 == 0);
    REQUIRE(indicesAreInRange(mesh));
  }
}

TEST_CASE("every primitive stands in the same unit box") {
  // The editor scales a model's footprint to one tile and stands its lowest
  // point on the ground. Building all four to these bounds is what makes a
  // dropped shape exactly a tile across rather than whatever its own maths
  // happened to produce.
  for (const MeshData& mesh : allPrimitives()) {
    REQUIRE(mesh.min.x == Approx(-0.5f).margin(1e-5f));
    REQUIRE(mesh.min.y == Approx(-0.5f).margin(1e-5f));
    REQUIRE(mesh.min.z == Approx(0.0f).margin(1e-5f));
    REQUIRE(mesh.max.x == Approx(0.5f).margin(1e-5f));
    REQUIRE(mesh.max.y == Approx(0.5f).margin(1e-5f));
    REQUIRE(mesh.max.z == Approx(1.0f).margin(1e-5f));
  }
}

TEST_CASE("every primitive's normals are unit length") {
  // Shading normalizes anyway, but a normal that is not one is a normal
  // that was never computed from the surface it belongs to.
  for (const MeshData& mesh : allPrimitives()) {
    for (const MeshVertex& vertex : mesh.vertices) {
      REQUIRE(Vec3::length(vertex.normal) == Approx(1.0f).margin(1e-4f));
    }
  }
}

TEST_CASE("a normal points away from the shape, never into it") {
  // Every one of these shapes is convex about the middle of its box, so a
  // normal facing outward is one that agrees with the direction its own
  // vertex sits from that centre.
  const Vec3 centre{0.0f, 0.0f, 0.5f};
  for (const MeshData& mesh : allPrimitives()) {
    for (const MeshVertex& vertex : mesh.vertices) {
      const Vec3 out = vertex.position - centre;
      REQUIRE(Vec3::dot(out, vertex.normal) >= -1e-4f);
    }
  }
}

TEST_CASE("the cube is six flat faces") {
  const MeshData cube = makeCubeMesh();
  // Four vertices and two triangles per face, with no vertex shared across
  // a corner: a shared one would have to carry one face's normal and would
  // round the edge it sits on.
  REQUIRE(cube.vertices.size() == 24);
  REQUIRE(cube.indices.size() == 36);
}

TEST_CASE("the sphere's surface is its own radius from the centre") {
  const MeshData sphere = makeSphereMesh();
  const Vec3 centre{0.0f, 0.0f, 0.5f};
  for (const MeshVertex& vertex : sphere.vertices) {
    REQUIRE(Vec3::length(vertex.position - centre) ==
            Approx(0.5f).margin(1e-5f));
  }
}

TEST_CASE("the pyramid comes to a point over the middle of its base") {
  const MeshData pyramid = makePyramidMesh();
  size_t apex_corners = 0;
  for (const MeshVertex& vertex : pyramid.vertices) {
    apex_corners += vertex.position.z > 0.5f ? 1 : 0;
  }
  // One apex vertex per side, each carrying that side's own normal.
  REQUIRE(apex_corners == 4);
}

TEST_CASE("the cylinder's side is smooth and its caps are not") {
  const MeshData cylinder = makeCylinderMesh();
  size_t flat_up = 0;
  for (const MeshVertex& vertex : cylinder.vertices) {
    flat_up += vertex.normal.z > 0.9f ? 1 : 0;
  }
  // The top cap's vertices face straight up; the side's face outward, with
  // no vertical component at all.
  REQUIRE(flat_up == MESH_PRIMITIVE_SEGMENTS * 3);
}
