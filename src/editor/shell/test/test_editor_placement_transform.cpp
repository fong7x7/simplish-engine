#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-placement-transform.h>

using Catch::Approx;
using namespace eng::editor;

namespace {

/// Apply a transform to a point.
eng::Vec3 apply(const eng::Mat4& m, const eng::Vec3& p) {
  return {m(0, 0) * p.x + m(0, 1) * p.y + m(0, 2) * p.z + m(0, 3),
          m(1, 0) * p.x + m(1, 1) * p.y + m(1, 2) * p.z + m(1, 3),
          m(2, 0) * p.x + m(2, 1) * p.y + m(2, 2) * p.z + m(2, 3)};
}

/// An asset two units wide, one deep, four tall, sitting off the origin.
EditorAsset boxAsset() {
  EditorAsset asset;
  asset.name = "box";
  asset.min = {-1.0f, -0.5f, 2.0f};
  asset.max = {1.0f, 0.5f, 6.0f};
  return asset;
}

/// A placement of asset 0 on a tile, optionally turned.
EditorPlacement placementAt(WorldPoint position, eng::Vec3 rotation = {}) {
  EditorPlacement placement;
  placement.position = position;
  placement.rotation = rotation;
  return placement;
}

}  // namespace

TEST_CASE("the footprint is scaled to one tile") {
  const EditorAsset asset = boxAsset();
  const eng::Mat4 m = makePlacementTransform(asset, placementAt({0.0f, 0.0f}));

  const eng::Vec3 left = apply(m, {asset.min.x, 0.0f, 0.0f});
  const eng::Vec3 right = apply(m, {asset.max.x, 0.0f, 0.0f});
  REQUIRE(right.x - left.x == Approx(1.0f));
}

TEST_CASE("proportions are kept") {
  const EditorAsset asset = boxAsset();
  const eng::Mat4 m = makePlacementTransform(asset, placementAt({0.0f, 0.0f}));

  // Two wide by one deep by four tall, scaled by a half.
  const eng::Vec3 low = apply(m, asset.min);
  const eng::Vec3 high = apply(m, asset.max);
  REQUIRE(high.y - low.y == Approx(0.5f));
  REQUIRE(high.z - low.z == Approx(2.0f));
}

TEST_CASE("the model is centred on its tile") {
  const eng::Mat4 m =
      makePlacementTransform(boxAsset(), placementAt({3.0f, 7.0f}));
  const eng::Vec3 centre = apply(m, {0.0f, 0.0f, 0.0f});

  // Tile (3, 7) spans 3..4 and 7..8, so its centre is (3.5, 7.5).
  REQUIRE(centre.x == Approx(3.5f));
  REQUIRE(centre.y == Approx(7.5f));
}

TEST_CASE("the model rests on the ground plane") {
  const EditorAsset asset = boxAsset();
  const eng::Mat4 m = makePlacementTransform(asset, placementAt({0.0f, 0.0f}));

  // Its lowest point is 2 units up in object space; it must land on z = 0
  // rather than floating or sinking.
  REQUIRE(apply(m, asset.min).z == Approx(0.0f));
}

TEST_CASE("a placement above the ground lifts the model with it") {
  const EditorAsset asset = boxAsset();
  const eng::Mat4 m =
      makePlacementTransform(asset, placementAt({0.0f, 0.0f, 2.0f}));
  REQUIRE(apply(m, asset.min).z == Approx(2.0f));
}

TEST_CASE("a mesh with no measured bounds keeps its own units") {
  EditorAsset asset;
  const eng::Mat4 m = makePlacementTransform(asset, placementAt({0.0f, 0.0f}));
  // Scaling by 1/0 would send every vertex to infinity.
  REQUIRE(m(0, 0) == Approx(1.0f));
}

TEST_CASE("a turn about Z spins the model where it stands") {
  const EditorAsset asset = boxAsset();
  const eng::Mat4 m =
      makePlacementTransform(asset, placementAt({3.0f, 7.0f}, {0, 0, 90.0f}));

  // The pivot is the footprint centre at the resting height, so it lands on
  // the tile centre whatever the rotation is.
  const eng::Vec3 centre = apply(m, {0.0f, 0.0f, asset.min.z});
  REQUIRE(centre.x == Approx(3.5f));
  REQUIRE(centre.y == Approx(7.5f));

  // A quarter turn sends the model's +X to world +Y.
  const eng::Vec3 right = apply(m, {asset.max.x, 0.0f, asset.min.z});
  REQUIRE(right.x == Approx(3.5f));
  REQUIRE(right.y == Approx(8.0f));
}

TEST_CASE("a turn about X tips the model over") {
  const EditorAsset asset = boxAsset();
  const eng::Mat4 m =
      makePlacementTransform(asset, placementAt({0.0f, 0.0f}, {90.0f, 0, 0}));

  // What was four units tall now runs two tiles along world Y, and what was
  // up is now horizontal.
  const eng::Vec3 top = apply(m, {0.0f, 0.0f, asset.max.z});
  REQUIRE(top.z == Approx(0.0f).margin(1e-5));
  REQUIRE(top.y == Approx(-1.5f));
}

TEST_CASE("no rotation leaves the transform as it always was") {
  const EditorAsset asset = boxAsset();
  const eng::Mat4 turned =
      makePlacementTransform(asset, placementAt({2.0f, 1.0f}));
  REQUIRE(turned(0, 1) == Approx(0.0f));
  REQUIRE(turned(1, 0) == Approx(0.0f));
  REQUIRE(turned(0, 0) == Approx(0.5f));
}

TEST_CASE("world bounds contain the placed model") {
  const EditorAsset asset = boxAsset();
  const PlacementBounds bounds =
      placementWorldBounds(asset, placementAt({3.0f, 7.0f}));

  // Half a tile either side of the tile centre, one deep quarter either
  // side, and two tiles tall standing on the ground.
  REQUIRE(bounds.min.x == Approx(3.0f));
  REQUIRE(bounds.max.x == Approx(4.0f));
  REQUIRE(bounds.min.y == Approx(7.25f));
  REQUIRE(bounds.max.y == Approx(7.75f));
  REQUIRE(bounds.min.z == Approx(0.0f));
  REQUIRE(bounds.max.z == Approx(2.0f));
}

TEST_CASE("world bounds grow to hold a turned model") {
  const EditorAsset asset = boxAsset();
  const PlacementBounds bounds =
      placementWorldBounds(asset, placementAt({0.0f, 0.0f}, {0, 0, 90.0f}));

  // The one-tile width and the half-tile depth have swapped axes.
  REQUIRE(bounds.max.x - bounds.min.x == Approx(0.5f));
  REQUIRE(bounds.max.y - bounds.min.y == Approx(1.0f));
}

TEST_CASE("an unmeasured asset reports the unit box on its tile") {
  // Nothing has uploaded this asset's mesh, so its bounds are all zero. A
  // box of no size would be invisible in the viewport and unclickable.
  const PlacementBounds bounds =
      placementWorldBounds(EditorAsset{}, placementAt({4.0f, 2.0f}));
  REQUIRE(bounds.min.x == Approx(4.0f));
  REQUIRE(bounds.max.x == Approx(5.0f));
  REQUIRE(bounds.min.y == Approx(2.0f));
  REQUIRE(bounds.max.y == Approx(3.0f));
  REQUIRE(bounds.max.z == Approx(1.0f));
}

TEST_CASE("a scale of two doubles the placed model in every direction") {
  const EditorAsset asset = boxAsset();
  EditorPlacement placement = placementAt({3.0f, 7.0f});
  const PlacementBounds normal = placementWorldBounds(asset, placement);
  placement.scale = 2.0f;
  const PlacementBounds doubled = placementWorldBounds(asset, placement);

  REQUIRE(doubled.max.x - doubled.min.x ==
          Approx((normal.max.x - normal.min.x) * 2.0f));
  REQUIRE(doubled.max.y - doubled.min.y ==
          Approx((normal.max.y - normal.min.y) * 2.0f));
  REQUIRE(doubled.max.z - doubled.min.z ==
          Approx((normal.max.z - normal.min.z) * 2.0f));
}

TEST_CASE("a scaled model still stands on its tile") {
  // Scaled about the same point rotation turns about — the footprint's
  // centre at ground level — so it grows up and out, not into the floor or
  // off across the grid.
  const EditorAsset asset = boxAsset();
  EditorPlacement placement = placementAt({3.0f, 7.0f});
  placement.scale = 3.0f;
  const PlacementBounds bounds = placementWorldBounds(asset, placement);

  REQUIRE(bounds.min.z == Approx(0.0f).margin(1e-5));
  REQUIRE((bounds.min.x + bounds.max.x) * 0.5f == Approx(3.5f));
  REQUIRE((bounds.min.y + bounds.max.y) * 0.5f == Approx(7.5f));
}

TEST_CASE("scale and rotation compose about the same point") {
  const EditorAsset asset = boxAsset();
  EditorPlacement placement = placementAt({0.0f, 0.0f}, {0, 0, 90.0f});
  placement.scale = 2.0f;
  const PlacementBounds bounds = placementWorldBounds(asset, placement);

  // The one-tile width and half-tile depth swap axes under the turn, and
  // both double.
  REQUIRE(bounds.max.x - bounds.min.x == Approx(1.0f));
  REQUIRE(bounds.max.y - bounds.min.y == Approx(2.0f));
}

TEST_CASE("an unmeasured asset's stand-in box scales with the placement") {
  // Picked and collided with at the size it will draw, before its mesh has
  // loaded as well as after.
  EditorPlacement placement = placementAt({4.0f, 2.0f});
  placement.scale = 2.0f;
  const PlacementBounds bounds = placementWorldBounds(EditorAsset{}, placement);
  REQUIRE(bounds.max.x - bounds.min.x == Approx(2.0f));
  REQUIRE(bounds.max.z - bounds.min.z == Approx(2.0f));
  REQUIRE((bounds.min.x + bounds.max.x) * 0.5f == Approx(4.5f));
}
