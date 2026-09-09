#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <engine/render-mesh/obj-loader.h>
#include <string_view>

using Catch::Approx;
using namespace eng;

namespace {

/// One triangle in the XY plane, with explicit normals.
constexpr std::string_view TRIANGLE = R"obj(
# a comment
v 0 0 0
v 1 0 0
v 0 1 0
vn 0 0 1
f 1//1 2//1 3//1
)obj";

/// The same triangle with no normals declared at all.
constexpr std::string_view TRIANGLE_NO_NORMALS = R"obj(
v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)obj";

/// A unit quad, which has to come back as two triangles.
constexpr std::string_view QUAD = R"obj(
v 0 0 0
v 1 0 0
v 1 1 0
v 0 1 0
vn 0 0 1
f 1//1 2//1 3//1 4//1
)obj";

}  // namespace

TEST_CASE("a triangle parses to three vertices and three indices") {
  const auto mesh = parseObjMesh(TRIANGLE);
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->vertices.size() == 3);
  REQUIRE(mesh->indices.size() == 3);
}

TEST_CASE("positions survive the round trip") {
  const auto mesh = parseObjMesh(TRIANGLE);
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->vertices[1].position.x == Approx(1.0f));
  REQUIRE(mesh->vertices[2].position.y == Approx(1.0f));
}

TEST_CASE("declared normals are used as declared") {
  const auto mesh = parseObjMesh(TRIANGLE);
  REQUIRE(mesh.has_value());
  for (const auto& vertex : mesh->vertices) {
    REQUIRE(vertex.normal.z == Approx(1.0f));
  }
}

TEST_CASE("a face without normals gets a computed flat normal") {
  const auto mesh = parseObjMesh(TRIANGLE_NO_NORMALS);
  REQUIRE(mesh.has_value());
  // Counter-clockwise in the XY plane faces +Z. Without this the mesh would
  // render black rather than shaded.
  REQUIRE(mesh->vertices[0].normal.z == Approx(1.0f));
}

TEST_CASE("a quad is triangulated as a fan") {
  const auto mesh = parseObjMesh(QUAD);
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->indices.size() == 6);
  // All four corners share one normal, so no vertex is duplicated.
  REQUIRE(mesh->vertices.size() == 4);
}

TEST_CASE("vertices are shared between faces that agree") {
  const auto mesh = parseObjMesh(QUAD);
  REQUIRE(mesh.has_value());
  // The fan reuses corner 0 in both triangles.
  REQUIRE(mesh->indices[0] == mesh->indices[3]);
}

TEST_CASE("bounds cover the geometry") {
  const auto mesh = parseObjMesh(QUAD);
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->min.x == Approx(0.0f));
  REQUIRE(mesh->min.y == Approx(0.0f));
  REQUIRE(mesh->max.x == Approx(1.0f));
  REQUIRE(mesh->max.y == Approx(1.0f));
}

TEST_CASE("negative indices count back from what was declared") {
  const auto mesh = parseObjMesh(R"obj(
v 0 0 0
v 1 0 0
v 0 1 0
f -3 -2 -1
)obj");
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->vertices.size() == 3);
  REQUIRE(mesh->vertices[1].position.x == Approx(1.0f));
}

TEST_CASE("every corner form parses") {
  // `a`, `a/b`, `a//c`, and `a/b/c` all appear in files in the wild.
  const auto mesh = parseObjMesh(R"obj(
v 0 0 0
v 1 0 0
v 0 1 0
vn 0 0 1
f 1 2/5 3//1
)obj");
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->indices.size() == 3);
}

TEST_CASE("texture coordinates and materials are skipped") {
  const auto mesh = parseObjMesh(R"obj(
mtllib scene.mtl
o Cube
usemtl Stone
s off
vt 0.5 0.5
v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)obj");
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->vertices.size() == 3);
}

TEST_CASE("text with no faces yields nothing") {
  REQUIRE_FALSE(parseObjMesh("v 0 0 0\nv 1 0 0\n").has_value());
  REQUIRE_FALSE(parseObjMesh("").has_value());
}

TEST_CASE("a face with fewer than three corners is dropped") {
  REQUIRE_FALSE(parseObjMesh("v 0 0 0\nv 1 0 0\nf 1 2\n").has_value());
}

TEST_CASE("windows line endings parse") {
  const auto mesh =
      parseObjMesh("v 0 0 0\r\nv 1 0 0\r\nv 0 1 0\r\nf 1 2 3\r\n");
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->vertices.size() == 3);
}

TEST_CASE("loading a file that does not exist fails cleanly") {
  REQUIRE_FALSE(loadObjMesh("/nonexistent/path/to/model.obj").has_value());
}

TEST_CASE("texture coordinates are read onto the vertices") {
  const auto mesh = parseObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                 "vt 0 0\nvt 1 0\nvt 0 1\n"
                                 "vn 0 0 1\n"
                                 "f 1/1/1 2/2/1 3/3/1\n");
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->vertices.size() == 3);
  REQUIRE(mesh->vertices[0].uv.x == 0.0f);
  REQUIRE(mesh->vertices[1].uv.x == 1.0f);
}

TEST_CASE("V is flipped on the way in") {
  // OBJ counts V up from the bottom of the image; every API this targets
  // counts it down from the top. Flipping once here beats flipping in each
  // shader that samples.
  const auto mesh = parseObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                 "vt 0 0\nvt 0 0.25\nvt 0 1\n"
                                 "vn 0 0 1\n"
                                 "f 1/1/1 2/2/1 3/3/1\n");
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->vertices[0].uv.y == 1.0f);
  REQUIRE(mesh->vertices[1].uv.y == 0.75f);
  REQUIRE(mesh->vertices[2].uv.y == 0.0f);
}

TEST_CASE("a model with no texture coordinates gets zeroes") {
  const auto mesh =
      parseObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n");
  REQUIRE(mesh.has_value());
  for (const auto& vertex : mesh->vertices) {
    REQUIRE(vertex.uv.x == 0.0f);
    REQUIRE(vertex.uv.y == 0.0f);
  }
}

TEST_CASE("corners differing only in texture coordinate are not shared") {
  // The seam of a UV map is exactly this: one position, one normal, two
  // texture coordinates. Sharing them would drag the seam across the face.
  const auto mesh = parseObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\n"
                                 "vt 0 0\nvt 1 0\n"
                                 "vn 0 0 1\n"
                                 "f 1/1/1 2/1/1 3/1/1\n"
                                 "f 1/2/1 2/1/1 4/1/1\n");
  REQUIRE(mesh.has_value());
  // Position 1 appears with two different coordinates, so it is two
  // vertices; the other three are shared across the faces that meet there.
  REQUIRE(mesh->vertices.size() == 5);
}

TEST_CASE("corners agreeing on all three indices are still shared") {
  const auto mesh = parseObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\n"
                                 "vt 0 0\n"
                                 "vn 0 0 1\n"
                                 "f 1/1/1 2/1/1 3/1/1\n"
                                 "f 2/1/1 4/1/1 3/1/1\n");
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->vertices.size() == 4);
}

TEST_CASE("a material library and material are recorded") {
  const auto mesh = parseObjMesh("mtllib crate.mtl\nusemtl body\n"
                                 "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->material_library == "crate.mtl");
  REQUIRE(mesh->material == "body");
  // Parsing does not touch disk, so nothing is resolved here.
  REQUIRE(mesh->texture_path.empty());
}

TEST_CASE("the first material is the one recorded") {
  // The mesh draws in one call with one map, so a model switching material
  // partway through takes the first — see mesh-data.h.
  const auto mesh =
      parseObjMesh("usemtl body\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"
                   "usemtl glass\nf 1 2 3\n");
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->material == "body");
}

TEST_CASE("a model naming no material records none") {
  const auto mesh = parseObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->material_library.empty());
  REQUIRE(mesh->material.empty());
}

TEST_CASE("the vertex layout is the size the backends describe") {
  // A backend's vertex descriptor restates this layout as a stride and a
  // set of offsets, and cannot include this header to check. This is what
  // fails when a field is added here and not there.
  REQUIRE(MESH_VERTEX_BYTES == 32);
  REQUIRE(offsetof(MeshVertex, normal) == 12);
  REQUIRE(offsetof(MeshVertex, uv) == 24);
}
