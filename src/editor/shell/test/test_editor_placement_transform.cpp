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

}  // namespace

TEST_CASE("the footprint is scaled to one tile") {
  const EditorAsset asset = boxAsset();
  const eng::Mat4 m = makePlacementTransform(asset, {0.0f, 0.0f});

  const eng::Vec3 left = apply(m, {asset.min.x, 0.0f, 0.0f});
  const eng::Vec3 right = apply(m, {asset.max.x, 0.0f, 0.0f});
  REQUIRE(right.x - left.x == Approx(1.0f));
}

TEST_CASE("proportions are kept") {
  const EditorAsset asset = boxAsset();
  const eng::Mat4 m = makePlacementTransform(asset, {0.0f, 0.0f});

  // Two wide by one deep by four tall, scaled by a half.
  const eng::Vec3 low = apply(m, asset.min);
  const eng::Vec3 high = apply(m, asset.max);
  REQUIRE(high.y - low.y == Approx(0.5f));
  REQUIRE(high.z - low.z == Approx(2.0f));
}

TEST_CASE("the model is centred on its tile") {
  const eng::Mat4 m = makePlacementTransform(boxAsset(), {3.0f, 7.0f});
  const eng::Vec3 centre = apply(m, {0.0f, 0.0f, 0.0f});

  // Tile (3, 7) spans 3..4 and 7..8, so its centre is (3.5, 7.5).
  REQUIRE(centre.x == Approx(3.5f));
  REQUIRE(centre.y == Approx(7.5f));
}

TEST_CASE("the model rests on the ground plane") {
  const EditorAsset asset = boxAsset();
  const eng::Mat4 m = makePlacementTransform(asset, {0.0f, 0.0f});

  // Its lowest point is 2 units up in object space; it must land on z = 0
  // rather than floating or sinking.
  REQUIRE(apply(m, asset.min).z == Approx(0.0f));
}

TEST_CASE("a placement above the ground lifts the model with it") {
  const EditorAsset asset = boxAsset();
  const eng::Mat4 m = makePlacementTransform(asset, {0.0f, 0.0f, 2.0f});
  REQUIRE(apply(m, asset.min).z == Approx(2.0f));
}

TEST_CASE("a mesh with no measured bounds keeps its own units") {
  EditorAsset asset;
  const eng::Mat4 m = makePlacementTransform(asset, {0.0f, 0.0f});
  // Scaling by 1/0 would send every vertex to infinity.
  REQUIRE(m(0, 0) == Approx(1.0f));
}
