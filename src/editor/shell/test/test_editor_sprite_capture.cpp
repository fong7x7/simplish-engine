#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-placement-transform.h>
#include <editor/shell/editor-sprite-ops.h>
#include <editor/shell/editor-sprite-transform.h>
#include <editor/shell/iso-camera.h>
#include <editor/shell/iso-view-matrix.h>
#include <editor/shell/mesh-rasterizer.h>
#include <engine/render-mesh/mesh-transform.h>
#include <engine/render-mesh/obj-loader.h>
#include <engine/render-sprite/sprite-quad.h>
#include <string_view>
#include <vector>

// ADR-003's interleave, through the CPU rasterizer: a billboard's quad is
// built, turned to face the camera and drawn against the same depth buffer
// the meshes use, so a sprite behind a cube is hidden by it and one in
// front of the same cube is not. The alpha-test cutout that gives the
// sprite its edges is a shader, and is covered on the GPU by
// test_gpu_sprite_cutout.cpp.

using namespace eng;
using namespace eng::editor;

namespace {

constexpr uint32_t CAPTURE_W = 320;
constexpr uint32_t CAPTURE_H = 320;

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

Rect captureRect() {
  return makeRect(0.0f, 0.0f, static_cast<float>(CAPTURE_W),
                  static_cast<float>(CAPTURE_H));
}

IsoCamera cameraWith(const IsoAxes& axes) {
  IsoCamera camera;
  camera.axes = axes;
  camera.focus = worldToIso(axes, {1.5f, 1.5f, 0.5f});
  return camera;
}

/// The screen position of a world point in a capture under @p axes.
IsoPoint screenOf(WorldPoint world, const IsoAxes& axes) {
  return worldToScreen(makeIsoView(cameraWith(axes), captureRect()), world);
}

/// A billboard a tile tall standing at @p at, showing one square frame.
EditorSprite billboardAt(WorldPoint at) {
  return makeEditorSprite("sprites/slime.png", at);
}

/// One capture of a scene of cubes and billboards.
class Capture {
public:
  explicit Capture(const IsoAxes& axes) : axes_(axes) {
    auto loaded = parseObjMesh(CUBE_OBJ);
    REQUIRE(loaded.has_value());
    cube_ = *loaded;
    orientYUpToZUp(cube_);
    cube_asset_.min = cube_.min;
    cube_asset_.max = cube_.max;
  }

  /// Stand a cube on the tile whose corner is @p tile.
  void addCube(WorldPoint tile) {
    EditorPlacement placement;
    placement.position = tile;
    draws_.push_back({&cube_, makePlacementTransform(cube_asset_, placement)});
  }

  /// Stand @p sprite's quad where it goes, sized for a square frame.
  void addSprite(const EditorSprite& sprite) {
    const float width = editorSpriteWidth(axes_, sprite, {64.0f, 64.0f});
    quads_.push_back(makeSpriteQuadMesh({}));
    transforms_.push_back(makeSpriteTransform(axes_, sprite, width));
  }

  /// Rasterize everything added so far.
  void render() {
    for (size_t i = 0; i < quads_.size(); ++i) {
      draws_.push_back({&quads_[i], transforms_[i]});
    }
    MeshRasterScene scene{};
    scene.view_projection =
        makeIsoViewProjection(makeIsoView(cameraWith(axes_), captureRect()),
                              {captureRect(), static_cast<float>(CAPTURE_W),
                               static_cast<float>(CAPTURE_H)});
    scene.draws = draws_;
    scene.width = CAPTURE_W;
    scene.height = CAPTURE_H;
    image_ = rasterizeMeshScene(scene);
  }

  /// The red channel at the pixel @p world lands on.
  [[nodiscard]] uint8_t redAt(WorldPoint world) const {
    const IsoPoint point = screenOf(world, axes_);
    const size_t offset = (static_cast<size_t>(point.y) * image_.width +
                           static_cast<size_t>(point.x)) *
                          4;
    return image_.pixels[offset];
  }

  /// Whether a pixel is background rather than geometry.
  [[nodiscard]] bool isBackground(uint32_t x, uint32_t y) const {
    const size_t offset = (static_cast<size_t>(y) * image_.width + x) * 4;
    return image_.pixels[offset] == 22 && image_.pixels[offset + 1] == 22 &&
           image_.pixels[offset + 2] == 26;
  }

  /// The pixel bounding box of everything drawn, as (width, height).
  [[nodiscard]] std::array<uint32_t, 2> silhouette() const {
    uint32_t min_x = image_.width, max_x = 0, min_y = image_.height, max_y = 0;
    for (uint32_t y = 0; y < image_.height; ++y) {
      for (uint32_t x = 0; x < image_.width; ++x) {
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

private:
  IsoAxes axes_;
  MeshData cube_{};
  EditorAsset cube_asset_{};
  std::vector<MeshData> quads_{};
  std::vector<Mat4> transforms_{};
  std::vector<MeshRasterScene::Draw> draws_{};
  ImageData image_{};
};

/// How far off a measured pixel size may be and still count as right: the
/// quad's edges land between pixels, so a couple either way is rounding.
constexpr uint32_t SILHOUETTE_SLOP = 2;

/// Whether @p measured is @p expected, to within that.
bool nearly(uint32_t measured, float expected) {
  const auto want = static_cast<uint32_t>(expected + 0.5f);
  return measured + SILHOUETTE_SLOP >= want &&
         want + SILHOUETTE_SLOP >= measured;
}

}  // namespace

// Req: ADR-003, 2026-09-09 amendment — a sprite's on-screen height is its
// world height times the cosine of the camera's pitch, and its width has to
// carry that same factor or a square frame draws as a rectangle.
TEST_CASE("a square frame draws square under either projection") {
  for (const IsoAxes axes : {ISO_AXES_DIMETRIC, ISO_AXES_ISOMETRIC}) {
    Capture capture(axes);
    capture.addSprite(billboardAt({1.5f, 1.5f, 0.0f}));
    capture.render();

    const auto size = capture.silhouette();
    // A billboard one tile tall is drawn as tall as a one-tile cube: the
    // projection's own rise, which is 42 px dimetric and 39 isometric.
    CHECK(nearly(size[1], axes.z_up));
    CHECK(nearly(size[0], axes.z_up));
  }
}

TEST_CASE("a taller billboard is taller on screen in proportion") {
  Capture capture(ISO_AXES_DIMETRIC);
  EditorSprite sprite = billboardAt({1.5f, 1.5f, 0.0f});
  sprite.height = 2.0f;
  capture.addSprite(sprite);
  capture.render();

  CHECK(nearly(capture.silhouette()[1], 2.0f * ISO_AXES_DIMETRIC.z_up));
}

// Req: ADR-003 — geometry and sprites interleave in one depth-buffered
// pass, so a character can walk behind a pillar and out again.
TEST_CASE("a billboard behind a cube is hidden by it, in front of it is not") {
  // The middle of the cube's camera-facing side, which is where all three
  // captures are read.
  const WorldPoint probe{1.5f, 2.0f, 0.5f};

  Capture alone(ISO_AXES_DIMETRIC);
  alone.addCube({1.0f, 1.0f, 0.0f});
  alone.render();
  const uint8_t cube_face = alone.redAt(probe);

  Capture behind(ISO_AXES_DIMETRIC);
  behind.addCube({1.0f, 1.0f, 0.0f});
  behind.addSprite(billboardAt({1.5f, 1.6f, 0.0f}));
  behind.render();

  Capture in_front(ISO_AXES_DIMETRIC);
  in_front.addCube({1.0f, 1.0f, 0.0f});
  in_front.addSprite(billboardAt({1.5f, 2.4f, 0.0f}));
  in_front.render();

  // The cube is opaque, so the billboard standing further from the camera
  // leaves that pixel exactly as the cube left it.
  CHECK(behind.redAt(probe) == cube_face);
  // The one standing nearer covers it, and is shaded as a surface facing
  // the camera rather than as the cube's side.
  CHECK(in_front.redAt(probe) != cube_face);
}
