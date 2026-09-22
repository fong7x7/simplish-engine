#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/render-sprite/sprite-sheet-frames.h>

using Catch::Approx;
using namespace eng;

namespace {

/// A sheet of @p columns by @p rows cells, playing @p frames of them at
/// twelve a second — the speed a dropped billboard starts at.
SpriteSheet sheetOf(uint16_t columns, uint16_t rows, uint16_t frames) {
  return {columns, rows, frames, 12.0f};
}

}  // namespace

TEST_CASE("a sheet that names no frame count plays every cell") {
  REQUIRE(spriteSheetFrameCount(sheetOf(4, 3, 0)) == 12);
}

TEST_CASE("a sheet plays the frames it names, not the cells it has") {
  // The case the count exists for: two cells of a 4x3 sheet are empty, and
  // playing them would blink the sprite out twice a loop.
  REQUIRE(spriteSheetFrameCount(sheetOf(4, 3, 10)) == 10);
}

TEST_CASE("a sheet cannot play more frames than it has cells") {
  REQUIRE(spriteSheetFrameCount(sheetOf(2, 2, 99)) == 4);
}

TEST_CASE("a blank sheet still shows one frame") {
  REQUIRE(spriteSheetFrameCount({0, 0, 0, 0.0f}) == 1);
}

TEST_CASE("frames advance with the clock and loop") {
  const SpriteSheet sheet = sheetOf(4, 1, 4);
  REQUIRE(spriteFrameAt(sheet, 0.0) == 0);
  REQUIRE(spriteFrameAt(sheet, 1.0 / 12.0) == 1);
  REQUIRE(spriteFrameAt(sheet, 3.5 / 12.0) == 3);
  // Back to the start on the fifth frame's worth of time.
  REQUIRE(spriteFrameAt(sheet, 4.0 / 12.0) == 0);
}

TEST_CASE("a long-running clock lands on the frame its own time says") {
  const SpriteSheet sheet = sheetOf(4, 1, 4);
  // An hour in, with no accumulated index to have drifted.
  REQUIRE(spriteFrameAt(sheet, 3600.0 + 2.0 / 12.0) == 2);
}

TEST_CASE("a sheet with no speed holds its first frame") {
  REQUIRE(spriteFrameAt({4, 1, 4, 0.0f}, 100.0) == 0);
}

TEST_CASE("frames are laid out left to right and then down") {
  const SpriteSheet sheet = sheetOf(2, 2, 4);
  const SpriteUvRect second = spriteFrameUv(sheet, 1);
  REQUIRE(second.u0 == Approx(0.5f));
  REQUIRE(second.v0 == Approx(0.0f));
  const SpriteUvRect third = spriteFrameUv(sheet, 2);
  REQUIRE(third.u0 == Approx(0.0f));
  REQUIRE(third.v0 == Approx(0.5f));
  REQUIRE(third.u1 == Approx(0.5f));
  REQUIRE(third.v1 == Approx(1.0f));
}

TEST_CASE("a frame past the last one wraps rather than leaving the image") {
  const SpriteUvRect wrapped = spriteFrameUv(sheetOf(2, 1, 2), 5);
  REQUIRE(wrapped.u0 == Approx(0.5f));
  REQUIRE(wrapped.u1 == Approx(1.0f));
}

TEST_CASE("the inset pulls every edge in by half a texel") {
  const SpriteUvRect inset =
      insetSpriteUv({0.0f, 0.5f, 0.5f, 1.0f}, 1.0f / 64.0f, 1.0f / 32.0f);
  REQUIRE(inset.u0 == Approx(0.5f / 64.0f));
  REQUIRE(inset.u1 == Approx(0.5f - 0.5f / 64.0f));
  REQUIRE(inset.v0 == Approx(0.5f + 0.5f / 32.0f));
  REQUIRE(inset.v1 == Approx(1.0f - 0.5f / 32.0f));
}
