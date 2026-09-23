#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <engine/render-ground/ground-mesh.h>

using Catch::Approx;
using namespace eng;

namespace {

/// The area the mesh covers seen from above, on the layer at @p z.
float areaAt(const MeshData& mesh, float z) {
  float area = 0.0f;
  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    const Vec3 a = mesh.vertices[mesh.indices[i]].position;
    const Vec3 b = mesh.vertices[mesh.indices[i + 1]].position;
    const Vec3 c = mesh.vertices[mesh.indices[i + 2]].position;
    if (std::abs(a.z - z) < 1e-6f) {
      area += ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) * 0.5f;
    }
  }
  return area;
}

/// A grid of the cells from (0, 0) to (@p width − 1, 0), each @p terrain.
GroundGrid row(int32_t width, uint8_t terrain) {
  GroundGrid grid;
  for (int32_t x = 0; x < width; ++x) {
    grid.set({x, 0}, terrain);
  }
  return grid;
}

}  // namespace

TEST_CASE("nothing painted is no geometry") {
  const MeshData mesh = makeGroundMesh(GroundGrid{}, 4);
  REQUIRE(mesh.vertices.empty());
  REQUIRE(mesh.indices.empty());
}

TEST_CASE("every triangle faces up, and so has positive area from above") {
  GroundGrid grid = row(3, 1);
  grid.set({1, 1}, 1);
  grid.set({3, 1}, 2);
  const MeshData mesh = makeGroundMesh(grid, 2);
  REQUIRE_FALSE(mesh.indices.empty());
  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    const Vec3 a = mesh.vertices[mesh.indices[i]].position;
    const Vec3 b = mesh.vertices[mesh.indices[i + 1]].position;
    const Vec3 c = mesh.vertices[mesh.indices[i + 2]].position;
    REQUIRE((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) > 0.0f);
  }
  for (const MeshVertex& vertex : mesh.vertices) {
    REQUIRE(vertex.normal.z == Approx(1.0f));
  }
}

TEST_CASE("a lone cell is a disc half a tile across") {
  GroundGrid grid;
  grid.set({4, 4}, 1);
  const MeshData mesh = makeGroundMesh(grid, 1);
  const float pi = 3.14159265f;
  // Chords under the arc make it a little less than the true disc.
  REQUIRE(areaAt(mesh, GROUND_LAYER_STEP) == Approx(pi * 0.25f).epsilon(0.02));
  REQUIRE(mesh.min.x == Approx(4.0f));
  REQUIRE(mesh.max.x == Approx(5.0f));
}

TEST_CASE("the middle of a painted area is whole cells, one quad each") {
  GroundGrid grid;
  for (int32_t y = 0; y < 3; ++y) {
    for (int32_t x = 0; x < 3; ++x) {
      grid.set({x, y}, 1);
    }
  }
  const MeshData mesh = makeGroundMesh(grid, 1);
  // Nine cells, with only the four outer corners rounded off.
  const float corners = 4.0f * 0.25f * (1.0f - 3.14159265f / 4.0f);
  REQUIRE(areaAt(mesh, GROUND_LAYER_STEP) ==
          Approx(9.0f - corners).epsilon(0.01));
}

TEST_CASE("each terrain is drawn a layer higher, over what it covers") {
  GroundGrid grid = row(4, 1);
  grid.set({1, 0}, 2);
  grid.set({2, 0}, 2);
  const MeshData mesh = makeGroundMesh(grid, 2);
  REQUIRE(mesh.max.z == Approx(2.0f * GROUND_LAYER_STEP));
  // Layer 1 runs under layer 2's cells wherever layer 2 does not cover
  // them whole, so there is no gap round layer 2's rounded ends.
  REQUIRE(areaAt(mesh, GROUND_LAYER_STEP) > 2.0f);
}

TEST_CASE("layer 1 is not drawn where layer 2 covers it whole") {
  GroundGrid grid;
  for (int32_t y = 0; y < 5; ++y) {
    for (int32_t x = 0; x < 5; ++x) {
      grid.set({x, y}, 2);
    }
  }
  const MeshData mesh = makeGroundMesh(grid, 2);
  // Under the interior of layer 2 there is nothing: only the rim, where
  // layer 2 rounds its corners off, needs layer 1 beneath it.
  REQUIRE(areaAt(mesh, GROUND_LAYER_STEP) < 1.0f);
}

TEST_CASE("texture coordinates stay inside the layer's own swatch") {
  GroundGrid grid = row(3, 1);
  grid.set({1, 0}, 3);
  const MeshData mesh = makeGroundMesh(grid, 3);
  for (const MeshVertex& vertex : mesh.vertices) {
    const auto layer =
        static_cast<int>(std::lround(vertex.position.z / GROUND_LAYER_STEP));
    REQUIRE(vertex.uv.x > 0.0f);
    REQUIRE(vertex.uv.x < 1.0f);
    REQUIRE(vertex.uv.y > static_cast<float>(layer - 1) / 3.0f);
    REQUIRE(vertex.uv.y < static_cast<float>(layer) / 3.0f);
  }
}
