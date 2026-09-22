#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <editor/shell/editor-sprite-ops.h>
#include <editor/shell/editor-sprite-transform.h>
#include <editor/shell/iso-projection.h>

using Catch::Approx;
using namespace eng;
using namespace eng::editor;

namespace {

/// A billboard standing in the middle of a tile, @p height tiles tall.
EditorSprite standing(float height) {
  EditorSprite sprite = makeEditorSprite("sprites/slime.png", {2.5f, 3.5f});
  sprite.height = height;
  return sprite;
}

/// Where @p transform puts the quad's corner at local (@p x, 0, @p z).
WorldPoint cornerOf(const Mat4& transform, float x, float z) {
  return {transform(0, 0) * x + transform(0, 2) * z + transform(0, 3),
          transform(1, 0) * x + transform(1, 2) * z + transform(1, 3),
          transform(2, 0) * x + transform(2, 2) * z + transform(2, 3)};
}

/// The normal the quad's local +Y carries through @p transform.
WorldPoint normalOf(const Mat4& transform) {
  return {transform(0, 1), transform(1, 1), transform(2, 1)};
}

}  // namespace

TEST_CASE("a square frame draws square, whatever the projection") {
  for (const IsoAxes axes : {ISO_AXES_DIMETRIC, ISO_AXES_ISOMETRIC}) {
    const EditorSprite sprite = standing(1.0f);
    const float width = editorSpriteWidth(axes, sprite, {64.0f, 64.0f});
    // Screen pixels across against screen pixels up: the two axes are not
    // at the same scale, and the width is what corrects for it.
    const float across = width * isoAcrossPixels(axes);
    const float up = sprite.height * axes.z_up;
    REQUIRE(across == Approx(up));
  }
}

TEST_CASE("a frame twice as wide as it is tall draws twice as wide") {
  const float square =
      editorSpriteWidth(ISO_AXES_DIMETRIC, standing(1.0f), {32.0f, 32.0f});
  const float wide =
      editorSpriteWidth(ISO_AXES_DIMETRIC, standing(1.0f), {64.0f, 32.0f});

  REQUIRE(wide == Approx(2.0f * square));
}

TEST_CASE("a sheet with no height leaves the billboard square") {
  REQUIRE(editorSpriteWidth(ISO_AXES_DIMETRIC, standing(2.0f), {0.0f, 0.0f}) ==
          Approx(2.0f));
}

TEST_CASE("the quad stands on the sprite's own base") {
  const EditorSprite sprite = standing(1.5f);
  const Mat4 transform = makeSpriteTransform(ISO_AXES_DIMETRIC, sprite, 1.0f);
  const WorldPoint base = cornerOf(transform, 0.0f, 0.0f);
  const WorldPoint top = cornerOf(transform, 0.0f, 1.0f);

  REQUIRE(base.x == Approx(2.5f));
  REQUIRE(base.y == Approx(3.5f));
  REQUIRE(base.z == Approx(0.0f));
  // Straight up the world, so the base stays where the depth is measured.
  REQUIRE(top.x == Approx(2.5f));
  REQUIRE(top.y == Approx(3.5f));
  REQUIRE(top.z == Approx(1.5f));
}

TEST_CASE("the quad widens along the screen, not along world X") {
  const Mat4 transform =
      makeSpriteTransform(ISO_AXES_ISOMETRIC, standing(1.0f), 2.0f);
  const WorldPoint right = cornerOf(transform, 0.5f, 0.0f);
  const IsoAxes axes = ISO_AXES_ISOMETRIC;
  const IsoPoint drawn = worldToIso(axes, right);
  const IsoPoint base = worldToIso(axes, {2.5f, 3.5f, 0.0f});

  // Level on the screen under a 45-degree yaw, which is the whole point of
  // turning the quad rather than laying it along world X.
  REQUIRE(drawn.y == Approx(base.y));
  REQUIRE(drawn.x > base.x);
}

TEST_CASE("the normal points at the camera, along the projection ray") {
  for (const IsoAxes axes : {ISO_AXES_DIMETRIC, ISO_AXES_ISOMETRIC}) {
    const Mat4 transform = makeSpriteTransform(axes, standing(1.0f), 1.0f);
    const WorldPoint normal = normalOf(transform);
    const WorldPoint ray = isoProjectionRay(axes);
    const float length =
        std::sqrt(ray.x * ray.x + ray.y * ray.y + ray.z * ray.z);

    REQUIRE(normal.x == Approx(ray.x / length).margin(1e-5));
    REQUIRE(normal.y == Approx(ray.y / length).margin(1e-5));
    REQUIRE(normal.z == Approx(ray.z / length).margin(1e-5));
  }
}
