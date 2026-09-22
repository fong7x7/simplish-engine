#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-sprite-quad-key.h>

using Catch::Approx;
using namespace eng;
using namespace eng::editor;

namespace {

/// A 4 by 2 sheet, 128 by 64 texels: frames of 32 by 32.
SpriteSheet testGrid() {
  return {4, 2, 8, 12.0f};
}

}  // namespace

TEST_CASE("two billboards on the same sheet and grid share every quad") {
  const EditorSpriteQuadKey a =
      makeEditorSpriteQuadKey(testGrid(), 3, {128.0f, 64.0f});
  const EditorSpriteQuadKey b =
      makeEditorSpriteQuadKey(testGrid(), 3, {128.0f, 64.0f});

  REQUIRE(a == b);
}

TEST_CASE("the frame count is no part of the key") {
  SpriteSheet fewer = testGrid();
  fewer.frames = 5;

  // Which cell a frame covers depends on the grid, not on how many of its
  // cells are played, so two billboards cut alike share quads whether or
  // not they play the same number of them.
  REQUIRE(makeEditorSpriteQuadKey(fewer, 2, {128.0f, 64.0f}) ==
          makeEditorSpriteQuadKey(testGrid(), 2, {128.0f, 64.0f}));
}

TEST_CASE("sheets of different sizes do not share a quad") {
  // The inset is measured in texels, so the same cell of a bigger image is
  // not the same rectangle.
  REQUIRE_FALSE(makeEditorSpriteQuadKey(testGrid(), 0, {128.0f, 64.0f}) ==
                makeEditorSpriteQuadKey(testGrid(), 0, {256.0f, 128.0f}));
}

TEST_CASE("a frame's rectangle is its own cell, pulled off the seam") {
  const SpriteUvRect uv = editorSpriteQuadUv(
      makeEditorSpriteQuadKey(testGrid(), 1, {128.0f, 64.0f}));

  // Frame 1 is the second cell across: u from a quarter to a half, inset
  // half a texel either side so bilinear sampling cannot reach frame 0.
  REQUIRE(uv.u0 == Approx(0.25f + 0.5f / 128.0f));
  REQUIRE(uv.u1 == Approx(0.5f - 0.5f / 128.0f));
  REQUIRE(uv.v0 == Approx(0.5f / 64.0f));
  REQUIRE(uv.v1 == Approx(0.5f - 0.5f / 64.0f));
}

TEST_CASE("a sheet of no known size is not pulled in at all") {
  const SpriteUvRect uv =
      editorSpriteQuadUv(makeEditorSpriteQuadKey(testGrid(), 0, {}));

  REQUIRE(uv.u0 == Approx(0.0f));
  REQUIRE(uv.u1 == Approx(0.25f));
}
