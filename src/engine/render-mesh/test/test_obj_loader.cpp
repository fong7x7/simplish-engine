#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
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
