#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-character-transform.h>
#include <game/player/player-system.h>

using Catch::Approx;
using namespace eng;
using namespace eng::editor;

namespace {

/// A model two units tall standing off-centre: its footprint is centred on
/// (3, 1), its feet at height 0.5.
EditorAsset tallModel() {
  EditorAsset asset;
  asset.min = {2.5f, 0.0f, 0.5f};
  asset.max = {3.5f, 2.0f, 2.5f};
  return asset;
}

/// @p point put through @p m.
Vec3 apply(const Mat4& m, Vec3 point) {
  return {m(0, 0) * point.x + m(0, 1) * point.y + m(0, 2) * point.z + m(0, 3),
          m(1, 0) * point.x + m(1, 1) * point.y + m(1, 2) * point.z + m(1, 3),
          m(2, 0) * point.x + m(2, 1) * point.y + m(2, 2) * point.z + m(2, 3)};
}

}  // namespace

TEST_CASE("a character stands on the feet, as tall as a player") {
  const Mat4 m =
      makeEditorCharacterTransform(tallModel(), {7.0f, 4.0f, 1.0f}, {1, 0});

  const Vec3 base = apply(m, {3.0f, 1.0f, 0.5f});
  const Vec3 top = apply(m, {3.0f, 1.0f, 2.5f});
  REQUIRE(base.x == Approx(7.0f));
  REQUIRE(base.y == Approx(4.0f));
  REQUIRE(base.z == Approx(1.0f));
  REQUIRE(top.z - base.z == Approx(game::PLAYER_HEIGHT_TILES));
}

TEST_CASE("a character's front, its own -Y, turns to face the aim") {
  const EditorAsset asset = tallModel();
  const Vec3 feet{0.0f, 0.0f, 0.0f};
  const Vec3 front{3.0f, 0.0f, 0.5f};

  const Vec3 facing_x =
      apply(makeEditorCharacterTransform(asset, feet, {1, 0}), front);
  const Vec3 facing_y =
      apply(makeEditorCharacterTransform(asset, feet, {0, 2}), front);

  REQUIRE(facing_x.x > 0.0f);
  REQUIRE(facing_x.y == Approx(0.0f).margin(1e-5));
  REQUIRE(facing_y.y > 0.0f);
  REQUIRE(facing_y.x == Approx(0.0f).margin(1e-5));
}

TEST_CASE("no aim faces +X, and an unmeasured model keeps its size") {
  const EditorAsset unmeasured;
  const Mat4 m = makeEditorCharacterTransform(unmeasured, {1, 1, 0}, {0, 0});
  const Vec3 front = apply(m, {0.0f, -1.0f, 0.0f});
  REQUIRE(front.x == Approx(2.0f));
  REQUIRE(front.y == Approx(1.0f));
}
