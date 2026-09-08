#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <editor/shell/editor-asset-thumbnail.h>
#include <engine/gui/gui-software-rasterizer.h>
#include <engine/render-mesh/mesh-data.h>
#include <engine/render-mesh/mesh-transform.h>
#include <engine/render-mesh/obj-loader.h>
#include <string>
#include <vector>

using namespace eng::editor;

namespace {

constexpr uint32_t SIZE = 64;

/// Add one triangle, with a normal facing the light so it shades brightly.
void addTriangle(eng::MeshData& mesh, const eng::Vec3& a, const eng::Vec3& b,
                 const eng::Vec3& c) {
  const auto base = static_cast<uint32_t>(mesh.vertices.size());
  const eng::Vec3 normal{0.0f, -1.0f, 0.0f};
  mesh.vertices.push_back({a, normal});
  mesh.vertices.push_back({b, normal});
  mesh.vertices.push_back({c, normal});
  mesh.indices.push_back(base);
  mesh.indices.push_back(base + 1);
  mesh.indices.push_back(base + 2);
}

/// A unit box's worth of geometry, as two triangles standing upright.
eng::MeshData makeQuad(float half) {
  eng::MeshData mesh;
  addTriangle(mesh, {-half, 0.0f, 0.0f}, {half, 0.0f, 0.0f},
              {half, 0.0f, 2.0f * half});
  addTriangle(mesh, {-half, 0.0f, 0.0f}, {half, 0.0f, 2.0f * half},
              {-half, 0.0f, 2.0f * half});
  mesh.min = {-half, 0.0f, 0.0f};
  mesh.max = {half, 0.0f, 2.0f * half};
  return mesh;
}

/// Whether the pixel at (x, y) differs from the image's corner, which the
/// model never reaches.
bool painted(const eng::ImageData& image, uint32_t x, uint32_t y) {
  const size_t offset = (static_cast<size_t>(y) * image.width + x) * 4;
  for (size_t i = 0; i < 3; ++i) {
    if (image.pixels[offset + i] != image.pixels[i]) {
      return true;
    }
  }
  return false;
}

/// Count how many pixels the model covers.
size_t paintedPixels(const eng::ImageData& image) {
  size_t count = 0;
  for (uint32_t y = 0; y < image.height; ++y) {
    for (uint32_t x = 0; x < image.width; ++x) {
      count += painted(image, x, y) ? 1 : 0;
    }
  }
  return count;
}

/// A unit cube as an OBJ file's text, Y-up as every exporter writes it.
constexpr const char* CUBE_OBJ = R"(
v -0.5 -0.5 -0.5
v  0.5 -0.5 -0.5
v  0.5  0.5 -0.5
v -0.5  0.5 -0.5
v -0.5 -0.5  0.5
v  0.5 -0.5  0.5
v  0.5  0.5  0.5
v -0.5  0.5  0.5
f 1 2 6 5
f 4 8 7 3
f 5 6 7 8
f 1 4 3 2
f 1 5 8 4
f 2 3 7 6
)";

}  // namespace

TEST_CASE("a thumbnail comes out at the size asked for") {
  const eng::ImageData image = renderAssetThumbnail(makeQuad(1.0f), SIZE);

  REQUIRE(image.width == SIZE);
  REQUIRE(image.height == SIZE);
  REQUIRE(image.pixels.size() == static_cast<size_t>(SIZE) * SIZE * 4);
}

TEST_CASE("a thumbnail actually draws the model") {
  const eng::ImageData image = renderAssetThumbnail(makeQuad(1.0f), SIZE);

  REQUIRE(paintedPixels(image) > 0);
}

TEST_CASE("a mesh with no triangles yields an empty image") {
  const eng::MeshData empty;

  // Empty, not blank: the caller has to tell "nothing to draw" from "drew
  // nothing", because one is a broken asset and the other is not.
  const eng::ImageData image = renderAssetThumbnail(empty, SIZE);
  REQUIRE(image.width == 0);
  REQUIRE(image.pixels.empty());
}

TEST_CASE("a zero size yields an empty image") {
  const eng::ImageData image = renderAssetThumbnail(makeQuad(1.0f), 0);

  REQUIRE(image.pixels.empty());
}

TEST_CASE("the model is fitted to the frame, not to its own units") {
  // Two models of wildly different scale should fill the frame alike; that
  // is the whole point of fitting rather than placing.
  const eng::ImageData small = renderAssetThumbnail(makeQuad(0.01f), SIZE);
  const eng::ImageData large = renderAssetThumbnail(makeQuad(100.0f), SIZE);

  const size_t small_pixels = paintedPixels(small);
  const size_t large_pixels = paintedPixels(large);
  REQUIRE(small_pixels > 0);
  const size_t difference = small_pixels > large_pixels
                                ? small_pixels - large_pixels
                                : large_pixels - small_pixels;
  REQUIRE(difference <= small_pixels / 10);
}

TEST_CASE("the model is centred in the frame") {
  const eng::ImageData image = renderAssetThumbnail(makeQuad(1.0f), SIZE);

  size_t left = 0;
  size_t right = 0;
  for (uint32_t y = 0; y < SIZE; ++y) {
    for (uint32_t x = 0; x < SIZE; ++x) {
      if (!painted(image, x, y)) {
        continue;
      }
      (x < SIZE / 2 ? left : right) += 1;
    }
  }
  REQUIRE(left > 0);
  REQUIRE(right > 0);
  const size_t difference = left > right ? left - right : right - left;
  REQUIRE(difference <= left / 4);
}

TEST_CASE("the model does not touch the frame's edge") {
  const eng::ImageData image = renderAssetThumbnail(makeQuad(1.0f), SIZE);

  // The fill fraction leaves a margin, so a card never shows a model
  // running off its own picture.
  for (uint32_t i = 0; i < SIZE; ++i) {
    REQUIRE_FALSE(painted(image, i, 0));
    REQUIRE_FALSE(painted(image, i, SIZE - 1));
    REQUIRE_FALSE(painted(image, 0, i));
    REQUIRE_FALSE(painted(image, SIZE - 1, i));
  }
}

TEST_CASE("a flat model still renders rather than dividing by zero") {
  eng::MeshData flat;
  // Every corner on one line: the projected span collapses on one axis.
  addTriangle(flat, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f});
  flat.min = {0.0f, 0.0f, 0.0f};
  flat.max = {2.0f, 0.0f, 0.0f};

  const eng::ImageData image = renderAssetThumbnail(flat, SIZE);
  REQUIRE(image.width == SIZE);
}

TEST_CASE("a single-point model still renders") {
  eng::MeshData point;
  addTriangle(point, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
              {1.0f, 1.0f, 1.0f});
  point.min = {1.0f, 1.0f, 1.0f};
  point.max = {1.0f, 1.0f, 1.0f};

  const eng::ImageData image = renderAssetThumbnail(point, SIZE);
  REQUIRE(image.width == SIZE);
}

TEST_CASE("the same mesh renders the same picture every time") {
  const eng::MeshData mesh = makeQuad(1.0f);

  // The cache in front of this keys on the file, not on the pixels, so a
  // second render has to agree with the one that was stored.
  const eng::ImageData first = renderAssetThumbnail(mesh, SIZE);
  const eng::ImageData second = renderAssetThumbnail(mesh, SIZE);
  REQUIRE(first.pixels == second.pixels);
}

TEST_CASE("a parsed OBJ renders through to a thumbnail") {
  auto mesh = eng::parseObjMesh(CUBE_OBJ);
  REQUIRE(mesh.has_value());
  // The same two steps the editor takes before it uploads a dropped model,
  // so the picture on the card is the model as it will be placed.
  eng::orientYUpToZUp(*mesh);

  const eng::ImageData image =
      renderAssetThumbnail(*mesh, ASSET_THUMBNAIL_SIZE);
  REQUIRE(image.width == ASSET_THUMBNAIL_SIZE);
  REQUIRE(paintedPixels(image) > 0);

  // An artifact, not an assertion: a thumbnail is a picture, and the only
  // way to know it reads as a cube is to look at it.
  const bool written = eng::GuiSoftwareRasterizer::writePng(
      image, "editor-asset-thumbnail-capture.png");
  INFO("wrote thumbnail: " << written);
}
