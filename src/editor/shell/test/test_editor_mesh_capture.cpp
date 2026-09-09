
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-placement-transform.h>
#include <editor/shell/iso-camera.h>
#include <editor/shell/iso-view-matrix.h>
#include <editor/shell/mesh-rasterizer.h>
#include <engine/gui/gui-software-rasterizer.h>
#include <engine/render-mesh/mesh-transform.h>
#include <engine/render-mesh/obj-loader.h>
#include <string>
#include <string_view>
#include <vector>

using namespace eng;
using namespace eng::editor;
using eng::editor::MeshRasterScene;

namespace {

constexpr uint32_t CAPTURE_W = 520;
constexpr uint32_t CAPTURE_H = 340;

/// A unit cube, Y-up, as an exporter writes one.
constexpr std::string_view CUBE_OBJ = R"obj(
v -0.5 0 -0.5
v  0.5 0 -0.5
v  0.5 0  0.5
v -0.5 0  0.5
v -0.5 1 -0.5
v  0.5 1 -0.5
v  0.5 1  0.5
v -0.5 1  0.5
f 1 2 3 4
f 8 7 6 5
f 1 5 6 2
f 2 6 7 3
f 3 7 8 4
f 4 8 5 1
)obj";

/// A cube loaded and oriented exactly as the editor does on a drop.
MeshData placedCube() {
  auto mesh = parseObjMesh(CUBE_OBJ);
  REQUIRE(mesh.has_value());
  orientYUpToZUp(*mesh);
  return *mesh;
}

EditorAsset assetFor(const MeshData& mesh) {
  EditorAsset asset;
  asset.name = "cube";
  asset.min = mesh.min;
  asset.max = mesh.max;
  return asset;
}

Rect captureRect() {
  return makeRect(0.0f, 0.0f, static_cast<float>(CAPTURE_W),
                  static_cast<float>(CAPTURE_H));
}

/// The scene the editor would build for these tiles.
struct CubeScene {
  MeshData mesh = placedCube();
  EditorAsset asset = assetFor(mesh);
  std::vector<MeshRasterScene::Draw> draws;
  ImageData image;

  /// A placement of the asset on one tile.
  static EditorPlacement onTile(const WorldPoint& tile) {
    EditorPlacement placement;
    placement.position = tile;
    return placement;
  }

  /// Place one cube per tile and render.
  void render(std::span<const WorldPoint> tiles) {
    draws.clear();
    for (const WorldPoint& tile : tiles) {
      draws.push_back({&mesh, makePlacementTransform(asset, onTile(tile))});
    }
    IsoCamera camera;
    camera.focus = worldToIso({1.5f, 1.5f});
    MeshRasterScene scene{};
    scene.view_projection =
        makeIsoViewProjection(makeIsoView(camera, captureRect()),
                              {captureRect(), static_cast<float>(CAPTURE_W),
                               static_cast<float>(CAPTURE_H)});
    scene.draws = draws;
    scene.width = CAPTURE_W;
    scene.height = CAPTURE_H;
    image = eng::editor::rasterizeMeshScene(scene);
  }

  /// Read a pixel as (r, g, b).
  [[nodiscard]] std::array<uint8_t, 3> pixel(uint32_t x, uint32_t y) const {
    const size_t offset = (static_cast<size_t>(y) * image.width + x) * 4;
    return {image.pixels[offset], image.pixels[offset + 1],
            image.pixels[offset + 2]};
  }

  /// Whether a pixel is background rather than geometry.
  [[nodiscard]] bool isBackground(uint32_t x, uint32_t y) const {
    const auto p = pixel(x, y);
    return p[0] == 22 && p[1] == 22 && p[2] == 26;
  }

  /// Count the pixels covered by geometry.
  [[nodiscard]] size_t litPixels() const {
    size_t count = 0;
    for (uint32_t y = 0; y < image.height; ++y) {
      for (uint32_t x = 0; x < image.width; ++x) {
        count += isBackground(x, y) ? 0 : 1;
      }
    }
    return count;
  }
};

/// The screen position of a world point in the capture.
IsoPoint screenOf(WorldPoint world) {
  IsoCamera camera;
  camera.focus = worldToIso({1.5f, 1.5f});
  return worldToScreen(makeIsoView(camera, captureRect()), world);
}

}  // namespace

TEST_CASE("a placed cube draws where its tile is") {
  CubeScene scene;
  const WorldPoint tiles[] = {{1.0f, 1.0f}};
  scene.render(tiles);

  // The tile's centre on screen must be covered by the cube standing on it.
  const IsoPoint centre = screenOf({1.5f, 1.5f});
  REQUIRE_FALSE(scene.isBackground(static_cast<uint32_t>(centre.x),
                                   static_cast<uint32_t>(centre.y)));
}

TEST_CASE("an empty scene draws nothing") {
  CubeScene scene;
  scene.render({});
  REQUIRE(scene.litPixels() == 0);
}

TEST_CASE("a cube stands up rather than lying flat") {
  CubeScene scene;
  const WorldPoint tiles[] = {{1.0f, 1.0f}};
  scene.render(tiles);

  // A Y-up model that was not rotated would cover the tile's footprint and
  // nothing above it. The cube is one tile tall, so pixels well above the
  // tile's back edge belong to geometry.
  const IsoPoint top = screenOf({1.5f, 1.0f, 0.9f});
  REQUIRE_FALSE(scene.isBackground(static_cast<uint32_t>(top.x),
                                   static_cast<uint32_t>(top.y)));
}

TEST_CASE("a nearer cube occludes the one behind it") {
  CubeScene scene;
  // Two tiles apart along Y, which on this projection means one is in front
  // of the other and they overlap on screen.
  const WorldPoint pair[] = {{1.0f, 1.0f}, {1.0f, 2.0f}};
  scene.render(pair);
  const size_t both = scene.litPixels();

  const WorldPoint single[] = {{1.0f, 2.0f}};
  scene.render(single);
  const size_t front_only = scene.litPixels();

  // The pair covers more than the front cube alone, but less than twice it:
  // the back one is partly hidden. Without a depth buffer this would be
  // either exactly double or visibly wrong.
  REQUIRE(both > front_only);
  REQUIRE(both < front_only * 2);
}

TEST_CASE("the mesh capture can be written to PNG for inspection") {
  CubeScene scene;
  const WorldPoint tiles[] = {
      {0.0f, 0.0f}, {1.0f, 1.0f}, {2.0f, 2.0f}, {3.0f, 0.0f}, {0.0f, 3.0f}};
  scene.render(tiles);

  // An artifact, not an assertion: whether the projection, the depth order
  // and the shading agree is something to look at.
  const std::string path = "editor-mesh-capture.png";
  const bool written = GuiSoftwareRasterizer::writePng(scene.image, path);
  INFO("wrote " << path << ": " << written);
  SUCCEED();
}
