#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <editor/shell/editor-asset-thumbnail.h>
#include <editor/shell/editor-placement-transform.h>
#include <editor/shell/editor-shape.h>
#include <engine/gui/gui-software-rasterizer.h>
#include <string>
#include <vector>

using Catch::Approx;
using namespace eng;
using namespace eng::editor;

namespace {

/// The four thumbnails side by side in one image, which is easier to look
/// at than four files and shows them at the same scale.
ImageData shapeStrip(uint32_t size) {
  ImageData strip;
  strip.width = size * static_cast<uint32_t>(EDITOR_SHAPE_COUNT);
  strip.height = size;
  strip.source_channels = 4;
  strip.pixels.assign(static_cast<size_t>(strip.width) * strip.height * 4, 0);
  for (size_t shape = 0; shape < EDITOR_SHAPE_COUNT; ++shape) {
    const ImageData tile =
        renderAssetThumbnail(makeEditorShapeMesh(EDITOR_SHAPE_KINDS[shape]),
                             size, ISO_AXES_DIMETRIC);
    for (uint32_t y = 0; y < size; ++y) {
      const size_t from = static_cast<size_t>(y) * size * 4;
      const size_t to =
          (static_cast<size_t>(y) * strip.width + shape * size) * 4;
      std::copy_n(tile.pixels.begin() + static_cast<ptrdiff_t>(from),
                  static_cast<size_t>(size) * 4,
                  strip.pixels.begin() + static_cast<ptrdiff_t>(to));
    }
  }
  return strip;
}

/// How many pixels of an image are not the background the rasterizer
/// clears to.
size_t paintedPixels(const ImageData& image) {
  size_t painted = 0;
  for (size_t i = 0; i + 3 < image.pixels.size(); i += 4) {
    const bool background = image.pixels[i] == 22 &&
                            image.pixels[i + 1] == 22 &&
                            image.pixels[i + 2] == 26;
    painted += background ? 0 : 1;
  }
  return painted;
}

}  // namespace

TEST_CASE("every shape is a mesh with geometry in it") {
  for (const EditorShapeKind kind : EDITOR_SHAPE_KINDS) {
    const MeshData mesh = makeEditorShapeMesh(kind);
    REQUIRE_FALSE(mesh.vertices.empty());
    REQUIRE(mesh.indices.size() % 3 == 0);
  }
}

TEST_CASE("a shape's asset carries a shape and no file") {
  std::vector<EditorAsset> assets;
  const size_t first = appendEditorShapeAssets(assets);

  REQUIRE(first == 0);
  REQUIRE(assets.size() == EDITOR_SHAPE_COUNT);
  for (const EditorAsset& asset : assets) {
    REQUIRE(asset.shape.has_value());
    // What marks it as generated rather than read: the loader is never
    // handed the empty path, because `shape` is asked about first.
    REQUIRE(asset.path.empty());
    REQUIRE_FALSE(asset.name.empty());
  }
}

TEST_CASE("shapes are appended after whatever assets are already there") {
  std::vector<EditorAsset> assets(3);
  const size_t first = appendEditorShapeAssets(assets);

  // A placement names a shape by this index, exactly as it names a scanned
  // model, so the shapes have to start past the last of them.
  REQUIRE(first == 3);
  REQUIRE(assets.size() == 3 + EDITOR_SHAPE_COUNT);
  REQUIRE(assets[3].shape == EDITOR_SHAPE_KINDS[0]);
}

TEST_CASE("a shape placed on a tile fills it, as a model would") {
  std::vector<EditorAsset> assets;
  appendEditorShapeAssets(assets);
  for (EditorAsset& asset : assets) {
    const MeshData mesh = makeEditorShapeMesh(*asset.shape);
    asset.min = mesh.min;
    asset.max = mesh.max;
    const PlacementBounds bounds =
        placementWorldBounds(asset, {.position = {2.0f, 3.0f}});
    // Its own tile, corner to corner: the shapes are built in a unit box,
    // and the placement transform scales a footprint to exactly one tile.
    REQUIRE(bounds.min.x == Approx(2.0f).margin(1e-4f));
    REQUIRE(bounds.max.x == Approx(3.0f).margin(1e-4f));
    REQUIRE(bounds.min.z == Approx(0.0f).margin(1e-4f));
  }
}

TEST_CASE("every shape renders a thumbnail with something in it") {
  for (const EditorShapeKind kind : EDITOR_SHAPE_KINDS) {
    const ImageData image = renderAssetThumbnail(
        makeEditorShapeMesh(kind), ASSET_THUMBNAIL_SIZE, ISO_AXES_DIMETRIC);
    REQUIRE(image.width == ASSET_THUMBNAIL_SIZE);
    REQUIRE(paintedPixels(image) > 0);
  }
}

TEST_CASE("the shape thumbnails can be written to PNG for inspection") {
  // An artifact, not an assertion: whether a sphere reads as a sphere and a
  // pyramid as a pyramid is something to look at.
  const ImageData strip = shapeStrip(ASSET_THUMBNAIL_SIZE);
  const std::string path = "editor-shape-capture.png";
  const bool written = GuiSoftwareRasterizer::writePng(strip, path);
  INFO("wrote " << path << ": " << written);
  SUCCEED();
}
