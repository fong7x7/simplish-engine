#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <engine/render-fx/fx-volume-quads.h>
#include <engine/render-fx/fx-world.h>

using Catch::Approx;
using namespace eng;

namespace {

/// A surface 200 pixels square under a camera that looks down z and leaves
/// world x and y as clip x and y, so a tile is a tile all the way through.
FxQuadView identityView() {
  return {Mat4::identity(), 200.0f, 200.0f};
}

/// Put one cloud of @p radius tiles at @p at into @p pool, past its
/// fade-in so it is at its thickest.
void addVolume(FxVolumePool& pool, Vec3 at, float radius) {
  const uint32_t i = pool.live++;
  pool.position[i] = at;
  pool.age[i] = 0.5f;
  pool.seed[i] = 3.0f;
  pool.scale[i] = 1.0f;
  FxVolume look;
  look.color = {0.1f, 0.1f, 0.1f, 1.0f};
  look.density = 2.0f;
  look.radius = radius;
  look.height = radius;
  look.life = 1.0f;
  pool.look[i] = look;
}

}  // namespace

TEST_CASE("a cloud is two triangles over the part of the frame its box "
          "covers",
          "[render-fx][volume-quads]") {
  FxVolumePool pool(4);
  addVolume(pool, {0.25f, 0.0f, 0.0f}, 0.25f);
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVolumeVertex> out;
  buildFxVolumeQuads(pool, identityView(), order, out);

  REQUIRE(out.size() == FX_VERTICES_PER_VOLUME);
  REQUIRE(out[0].clip[0] == Approx(0.0f).margin(0.01));
  REQUIRE(out[1].clip[0] == Approx(0.5f).margin(0.01));
  REQUIRE(out[0].clip[1] == Approx(-0.25f).margin(0.01));
  REQUIRE(out[2].clip[1] == Approx(0.25f).margin(0.01));
  // Its colour and thickness ride along, and its seed with them.
  REQUIRE(out[0].color[3] == Approx(1.0f));
  REQUIRE(out[0].params[1] == Approx(1.0f));
  REQUIRE(out[0].origin[3] == Approx(3.0f));
}

TEST_CASE("a corner's ray starts in the cloud's own space, where its box "
          "runs -1 to 1",
          "[render-fx][volume-quads]") {
  FxVolumePool pool(4);
  addVolume(pool, {0.0f, 0.0f, 0.0f}, 0.5f);
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVolumeVertex> out;
  buildFxVolumeQuads(pool, identityView(), order, out);

  // The rectangle's corners are the box's walls, a hair outside them.
  REQUIRE(out[0].origin[0] == Approx(-1.0f).margin(0.01));
  REQUIRE(out[0].origin[1] == Approx(-1.0f).margin(0.01));
  REQUIRE(out[2].origin[0] == Approx(1.0f).margin(0.01));
  REQUIRE(out[2].origin[1] == Approx(1.0f).margin(0.01));
  REQUIRE(std::abs(out[0].origin[0]) > 1.0f);
  // The camera looks down z, so the ray runs down the box's own z, and
  // one tile of it is two units of a box half a tile high.
  REQUIRE(out[0].ray[0] == Approx(0.0f));
  REQUIRE(out[0].ray[1] == Approx(0.0f));
  REQUIRE(std::abs(out[0].ray[2]) == Approx(2.0f));
}

TEST_CASE("clouds are laid out farthest first", "[render-fx][volume-quads]") {
  FxVolumePool pool(4);
  addVolume(pool, {0.0f, 0.0f, 0.2f}, 0.1f);
  addVolume(pool, {0.0f, 0.0f, 0.8f}, 0.1f);
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVolumeVertex> out;
  buildFxVolumeQuads(pool, identityView(), order, out);

  REQUIRE(out.size() == 2 * FX_VERTICES_PER_VOLUME);
  REQUIRE(out[0].clip[2] > out[FX_VERTICES_PER_VOLUME].clip[2]);
}

TEST_CASE("a cloud nowhere near the frame is left out",
          "[render-fx][volume-quads]") {
  FxVolumePool pool(4);
  addVolume(pool, {8.0f, 0.0f, 0.0f}, 0.25f);
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVolumeVertex> out;
  buildFxVolumeQuads(pool, identityView(), order, out);
  REQUIRE(out.empty());
}

TEST_CASE("a camera with no depth to march along draws no smoke",
          "[render-fx][volume-quads]") {
  FxVolumePool pool(4);
  addVolume(pool, {}, 0.25f);
  FxQuadView view = identityView();
  view.view_projection(2, 2) = 0.0f;
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVolumeVertex> out;
  buildFxVolumeQuads(pool, view, order, out);
  REQUIRE(out.empty());
}
