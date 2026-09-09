
#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-placement-transform.h>
#include <editor/shell/editor-shape.h>
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
using Catch::Approx;
using namespace eng::editor;
using eng::editor::MeshRasterScene;

namespace {

constexpr uint32_t CAPTURE_W = 520;
constexpr uint32_t CAPTURE_H = 340;
/// Height of a placed cube's top face: one tile, since the placement
/// transform scales a model's footprint to fill one.
constexpr float CUBE_TOP = 1.0f;

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

/// A light shining straight down, which is square on to a cube's top face.
MeshLight overheadLight(float intensity) {
  MeshLight light;
  light.direction = {0.0f, 0.0f, 1.0f};
  light.intensity = intensity;
  return light;
}

/// The camera these captures are taken with, under @p axes.
IsoCamera cameraWith(const IsoAxes& axes) {
  IsoCamera camera;
  camera.axes = axes;
  camera.focus = worldToIso(axes, {1.5f, 1.5f});
  return camera;
}

/// The screen position of a world point in a capture taken under @p axes.
IsoPoint screenOf(WorldPoint world, const IsoAxes& axes = ISO_AXES_DIMETRIC) {
  return worldToScreen(makeIsoView(cameraWith(axes), captureRect()), world);
}

/// The scene the editor would build for these tiles.
struct CubeScene {
  /// The projection the capture is taken under.
  IsoAxes axes = ISO_AXES_DIMETRIC;
  MeshData mesh = placedCube();
  EditorAsset asset = assetFor(mesh);
  std::vector<MeshRasterScene::Draw> draws;
  std::vector<MeshLight> lights;
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
    MeshRasterScene scene{};
    scene.view_projection =
        makeIsoViewProjection(makeIsoView(cameraWith(axes), captureRect()),
                              {captureRect(), static_cast<float>(CAPTURE_W),
                               static_cast<float>(CAPTURE_H)});
    scene.draws = draws;
    scene.lights = lights;
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

  /// How bright the top face of the cube on @p tile came out, as its red
  /// channel. The top face is the one every light in these tests is aimed
  /// at, and one channel is enough to compare two renders of it.
  [[nodiscard]] uint8_t topFaceRed(const WorldPoint& tile) const {
    const IsoPoint point = screenOf({tile.x + 0.5f, tile.y + 0.5f, CUBE_TOP});
    return pixel(static_cast<uint32_t>(point.x),
                 static_cast<uint32_t>(point.y))[0];
  }

  /// The pixel bounding box of everything drawn, as (width, height). Zero
  /// for an empty render.
  [[nodiscard]] std::array<uint32_t, 2> silhouette() const {
    uint32_t min_x = image.width, max_x = 0, min_y = image.height, max_y = 0;
    for (uint32_t y = 0; y < image.height; ++y) {
      for (uint32_t x = 0; x < image.width; ++x) {
        if (isBackground(x, y)) {
          continue;
        }
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
      }
    }
    return {max_x - min_x + 1, max_y - min_y + 1};
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

TEST_CASE("a placed cube draws where its tile is, isometrically too") {
  CubeScene scene;
  scene.axes = ISO_AXES_ISOMETRIC;
  const WorldPoint tiles[] = {{1.0f, 1.0f}};
  scene.render(tiles);

  const IsoPoint centre = screenOf({1.5f, 1.5f}, ISO_AXES_ISOMETRIC);
  REQUIRE_FALSE(scene.isBackground(static_cast<uint32_t>(centre.x),
                                   static_cast<uint32_t>(centre.y)));
}

TEST_CASE("the isometric projection turns the tile a different way") {
  // The same cube on the same tile, seen two ways. Under the isometric axes
  // the tile is a diamond rotated 45 degrees off the dimetric rectangle, so
  // one silhouette cannot be the other's.
  CubeScene dimetric;
  const WorldPoint tiles[] = {{1.0f, 1.0f}};
  dimetric.render(tiles);
  const size_t dimetric_pixels = dimetric.litPixels();

  CubeScene isometric;
  isometric.axes = ISO_AXES_ISOMETRIC;
  isometric.render(tiles);

  REQUIRE(isometric.litPixels() > 0);
  REQUIRE(isometric.litPixels() != dimetric_pixels);
}

TEST_CASE("depth sorts the same way under the isometric projection") {
  // The depth row is derived from the axes, so a sign slip there would
  // show up as the back cube drawing over the front one — visible only in
  // whichever projection was not the one the constants were written for.
  CubeScene scene;
  scene.axes = ISO_AXES_ISOMETRIC;
  const WorldPoint pair[] = {{1.0f, 1.0f}, {1.0f, 2.0f}};
  scene.render(pair);
  const size_t both = scene.litPixels();

  const WorldPoint single[] = {{1.0f, 2.0f}};
  scene.render(single);
  const size_t front_only = scene.litPixels();

  REQUIRE(both > front_only);
  REQUIRE(both < front_only * 2);
}

TEST_CASE("a sphere draws round in both projections") {
  // The projection has to keep shapes, not merely place them. It did not:
  // the height axis was a chosen constant rather than one derived from the
  // ground axes, which drew every sphere as an oval 1.25 (dimetric) or 1.5
  // (isometric) times taller than it was wide.
  for (const IsoAxes& axes : {ISO_AXES_DIMETRIC, ISO_AXES_ISOMETRIC}) {
    CubeScene scene;
    scene.mesh = makeEditorShapeMesh(EditorShapeKind::SPHERE);
    scene.asset = assetFor(scene.mesh);
    scene.axes = axes;
    const WorldPoint tiles[] = {{1.0f, 1.0f}};
    scene.render(tiles);

    const auto size = scene.silhouette();
    const float ratio =
        static_cast<float>(size[1]) / static_cast<float>(size[0]);
    INFO("silhouette " << size[0] << "x" << size[1]);
    // A pixel either way: the silhouette is measured off a rasterized
    // triangle mesh, not an analytic circle.
    REQUIRE(ratio == Approx(1.0f).margin(0.03f));
  }
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

TEST_CASE("a scene with no lights of its own is lit by the key light") {
  CubeScene scene;
  const WorldPoint tiles[] = {{1.0f, 1.0f}};
  scene.render(tiles);
  const uint8_t unlit = scene.topFaceRed({1.0f, 1.0f});

  // The key light comes over the viewer's shoulder, so a top face is well
  // lit but not fully: brighter than ambient, short of white.
  REQUIRE(unlit > 100);
  REQUIRE(unlit < 255);
}

TEST_CASE("a light brightens the face it points at") {
  CubeScene scene;
  const WorldPoint tiles[] = {{1.0f, 1.0f}};
  scene.render(tiles);
  const uint8_t before = scene.topFaceRed({1.0f, 1.0f});

  scene.lights = {overheadLight(2.0f)};
  scene.render(tiles);

  REQUIRE(scene.topFaceRed({1.0f, 1.0f}) > before);
}

TEST_CASE("a light aimed away leaves a face with the ambient alone") {
  CubeScene scene;
  const WorldPoint tiles[] = {{1.0f, 1.0f}};
  MeshLight from_below = overheadLight(1.0f);
  from_below.direction = {0.0f, 0.0f, -1.0f};
  scene.lights = {from_below};
  scene.render(tiles);

  // Nothing reaches the top face from underneath, so what is left is the
  // ambient floor: 0.38 of the surface colour's red.
  const auto ambient = static_cast<uint8_t>(0.74f * 0.38f * 255.0f);
  const uint8_t lit = scene.topFaceRed({1.0f, 1.0f});
  REQUIRE(lit >= ambient - 1);
  REQUIRE(lit <= ambient + 1);
}

TEST_CASE("a point light falls off before it reaches a distant tile") {
  CubeScene scene;
  const WorldPoint tiles[] = {{1.0f, 1.0f}, {4.0f, 1.0f}};
  MeshLight lamp;
  lamp.kind = MESH_LIGHT_POINT;
  // Over the first tile, and reaching about two tiles from there.
  lamp.position = {1.5f, 1.5f, 2.0f};
  lamp.range = 2.5f;
  lamp.intensity = 2.0f;
  scene.lights = {lamp};
  scene.render(tiles);

  REQUIRE(scene.topFaceRed({1.0f, 1.0f}) > scene.topFaceRed({4.0f, 1.0f}));
}

TEST_CASE("a coloured light tints what it lights") {
  CubeScene scene;
  const WorldPoint tiles[] = {{1.0f, 1.0f}};
  MeshLight red = overheadLight(2.0f);
  red.color = {1.0f, 0.0f, 0.0f};
  scene.lights = {red};
  scene.render(tiles);

  const IsoPoint centre = screenOf({1.5f, 1.5f, CUBE_TOP});
  const auto rgb = scene.pixel(static_cast<uint32_t>(centre.x),
                               static_cast<uint32_t>(centre.y));
  // Only the red channel takes the light; the others keep the ambient they
  // would have had anyway.
  REQUIRE(rgb[0] > rgb[1]);
  REQUIRE(rgb[1] > 0);
}

TEST_CASE("a light past the shader's list is not the renderer's to drop") {
  // The editor truncates to MESH_MAX_LIGHTS before the renderer sees them,
  // so the rasterizer shades however many it is handed — this is the CPU
  // path standing in for a GPU one that cannot loop past its array.
  CubeScene scene;
  const WorldPoint tiles[] = {{1.0f, 1.0f}};
  scene.lights.assign(MESH_MAX_LIGHTS + 2, overheadLight(0.1f));
  scene.render(tiles);

  REQUIRE(scene.litPixels() > 0);
}
