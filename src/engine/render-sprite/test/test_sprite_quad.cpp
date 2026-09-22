#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/render-sprite/sprite-quad.h>

using Catch::Approx;
using namespace eng;

TEST_CASE("the quad is a tile across, standing on its own base") {
  const MeshData quad = makeSpriteQuadMesh({});
  REQUIRE(quad.vertices.size() == 4);
  REQUIRE(quad.indices.size() == 6);
  REQUIRE(quad.min.x == Approx(-0.5f));
  REQUIRE(quad.max.x == Approx(0.5f));
  // Its base is the origin, so the model matrix puts it on the ground by
  // translation alone.
  REQUIRE(quad.min.z == Approx(0.0f));
  REQUIRE(quad.max.z == Approx(1.0f));
}

TEST_CASE("every corner is flat in the local X-Z plane") {
  const MeshData quad = makeSpriteQuadMesh({});
  for (const MeshVertex& vertex : quad.vertices) {
    REQUIRE(vertex.position.y == Approx(0.0f));
  }
}

TEST_CASE("the normal is local +Y, which carries no vertex") {
  const MeshData quad = makeSpriteQuadMesh({});
  for (const MeshVertex& vertex : quad.vertices) {
    REQUIRE(vertex.normal.x == Approx(0.0f));
    REQUIRE(vertex.normal.y == Approx(1.0f));
    REQUIRE(vertex.normal.z == Approx(0.0f));
  }
}

TEST_CASE("the frame's rectangle lands the right way up") {
  // V grows downward in an image, and Z grows upward in the world, so the
  // top of the frame belongs to the top of the quad.
  const MeshData quad = makeSpriteQuadMesh({0.25f, 0.5f, 0.5f, 1.0f});
  for (const MeshVertex& vertex : quad.vertices) {
    const float expected = vertex.position.z > 0.5f ? 0.5f : 1.0f;
    REQUIRE(vertex.uv.y == Approx(expected));
  }
}

TEST_CASE("the frame's rectangle lands the right way round") {
  const MeshData quad = makeSpriteQuadMesh({0.25f, 0.5f, 0.5f, 1.0f});
  for (const MeshVertex& vertex : quad.vertices) {
    const float expected = vertex.position.x > 0.0f ? 0.5f : 0.25f;
    REQUIRE(vertex.uv.x == Approx(expected));
  }
}
