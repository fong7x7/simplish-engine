#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/render-water/water-depth.h>
#include <engine/render-water/water-surface-mesh.h>

using Catch::Approx;
using namespace eng;

namespace {

/// Two rows of four water cells, the west two 0.25 tiles deep, clear and
/// red, and the east two 2 deep, opaque and blue.
WaterLayer twoWaters() {
  WaterLayer layer;
  for (int32_t x = 0; x < 4; ++x) {
    for (int32_t y = 0; y < 2; ++y) {
      const bool west = x < 2;
      setWaterCell(layer, {x, y},
                   {waterDepthUnits(west ? 0.25f : 2.0f),
                    static_cast<uint8_t>(west ? 255 : 0), 0,
                    static_cast<uint8_t>(west ? 0 : 255),
                    static_cast<uint8_t>(west ? 0 : 255)});
    }
  }
  return layer;
}

/// Whether @p vertex is the water's own rather than the wet band's.
bool isWater(const MeshVertex& vertex) {
  return vertex.uv.x > 0.0f;
}

}  // namespace

TEST_CASE("no water, no surface", "[render-water][surface]") {
  CHECK(makeWaterSurfaceMesh(WaterLayer{}).vertices.empty());
}

TEST_CASE("the water covers its cells alone, over every ground layer",
          "[render-water][surface]") {
  WaterLayer layer;
  setWaterCell(layer, {1, 0}, {.depth = 16});
  setWaterCell(layer, {2, 0}, {.depth = 16});
  const MeshData mesh = makeWaterSurfaceMesh(layer);
  REQUIRE_FALSE(mesh.indices.empty());
  for (const MeshVertex& vertex : mesh.vertices) {
    CHECK(vertex.position.z == WATER_SURFACE_HEIGHT);
    if (isWater(vertex)) {
      CHECK(vertex.position.x >= 1.0f);
      CHECK(vertex.position.x <= 3.0f);
    }
  }
  // Over the eight layers the ground may stack, under a tile's thickness.
  CHECK(WATER_SURFACE_HEIGHT > 8.0f * GROUND_LAYER_STEP);
  CHECK(WATER_SURFACE_HEIGHT < 1.0f / 32.0f);
}

TEST_CASE("a lone cell of water is a round pool", "[render-water][surface]") {
  WaterLayer layer;
  setWaterCell(layer, {5, 5}, {.depth = 16});
  const MeshData mesh = makeWaterSurfaceMesh(layer);
  REQUIRE_FALSE(mesh.vertices.empty());
  // Rounded quarters reach the middle of each edge and no corner.
  for (const MeshVertex& vertex : mesh.vertices) {
    if (!isWater(vertex)) {
      continue;
    }
    const float dx = vertex.position.x - 5.5f;
    const float dy = vertex.position.y - 5.5f;
    CHECK(dx * dx + dy * dy <= 0.25f + 1e-4f);
  }
}

TEST_CASE("the surface carries depth in u, opacity in v, colour as normal",
          "[render-water][surface]") {
  const MeshData mesh = makeWaterSurfaceMesh(twoWaters());
  REQUIRE_FALSE(mesh.vertices.empty());
  float shallowest = 99.0f;
  float deepest = 0.0f;
  bool between = false;
  for (const MeshVertex& vertex : mesh.vertices) {
    if (isWater(vertex)) {
      shallowest = std::min(shallowest, vertex.uv.x);
      deepest = std::max(deepest, vertex.uv.x);
      between |= vertex.uv.x > 0.3f && vertex.uv.x < 1.9f;
      CHECK(vertex.normal.x + vertex.normal.z == Approx(1.0f));
    }
  }
  CHECK(shallowest == Approx(0.25f));
  CHECK(deepest == Approx(2.0f));
  CHECK(between);
}

// Req: docs/engine/water.md §4 — the ground the water wets: a band a cell
// out all round, drawn before the water so the water goes over it, and
// carrying no water, which is how the shader knows it.
TEST_CASE("a wet band a cell wide is drawn first, under the water",
          "[render-water][surface]") {
  WaterLayer layer;
  setWaterCell(layer, {5, 5}, {.depth = 16});
  const MeshData mesh = makeWaterSurfaceMesh(layer);
  REQUIRE_FALSE(mesh.indices.empty());
  const MeshVertex& first = mesh.vertices[mesh.indices.front()];
  const MeshVertex& last = mesh.vertices[mesh.indices.back()];
  CHECK_FALSE(isWater(first));
  CHECK(isWater(last));
  float west = 99.0f;
  for (const MeshVertex& vertex : mesh.vertices) {
    west = std::min(west, vertex.position.x);
    CHECK((isWater(vertex) ||
           vertex.normal.x + vertex.normal.y + vertex.normal.z == 0.0f));
  }
  CHECK(west < 5.0f);
  CHECK(west >= 4.0f);
}
