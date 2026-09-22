#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <engine/render-fx/fx-quads.h>
#include <numbers>

using Catch::Approx;
using namespace eng;

namespace {

/// A surface 200 pixels square, under a matrix that leaves world
/// coordinates as clip coordinates — so a tenth of a tile is ten pixels.
FxQuadView identityView() {
  return {Mat4::identity(), 200.0f, 200.0f};
}

/// Put a particle into @p pool at @p at, moving at @p velocity, with a
/// radius of 0.1 tiles that stays put and a colour going from red to blue.
void addParticle(FxParticlePool& pool, Vec3 at, Vec3 velocity, float stretch) {
  const uint32_t i = pool.live++;
  pool.position[i] = at;
  pool.velocity[i] = velocity;
  pool.age[i] = 0.0f;
  pool.life[i] = 1.0f;
  pool.scale[i] = 1.0f;
  FxParticleLook look;
  look.size_start = 0.1f;
  look.size_end = 0.1f;
  look.color_start = {1.0f, 0.0f, 0.0f, 0.0f};
  look.color_end = {0.0f, 0.0f, 1.0f, 1.0f};
  look.stretch = stretch;
  pool.look[i] = look;
}

}  // namespace

TEST_CASE("a particle is a quad round in pixels, facing the camera at one "
          "depth",
          "[render-fx][quads]") {
  FxParticlePool pool(4);
  addParticle(pool, {0.5f, 0.0f, 0.25f}, {}, 0.0f);
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVertex> out;

  buildFxQuads(pool, identityView(), order, out);
  REQUIRE(out.size() == FX_VERTICES_PER_PARTICLE);
  for (const FxVertex& v : out) {
    // Ten pixels either side of its centre, which is 0.1 of clip space.
    CHECK(std::abs(v.clip[0] - 0.5f) == Approx(0.1f));
    CHECK(std::abs(v.clip[1]) == Approx(0.1f));
    CHECK(v.clip[2] == 0.25f);
    CHECK(v.clip[3] == 1.0f);
    CHECK(std::abs(v.uv[0]) == 1.0f);
    CHECK(v.color[0] == 1.0f);
  }
}

TEST_CASE("a particle's colour and size follow its age", "[render-fx][quads]") {
  FxParticlePool pool(1);
  addParticle(pool, {}, {}, 0.0f);
  pool.look[0].size_end = 0.3f;
  pool.age[0] = 0.5f;
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVertex> out;

  buildFxQuads(pool, identityView(), order, out);
  REQUIRE(!out.empty());
  CHECK(out[0].color[0] == Approx(0.5f));
  CHECK(out[0].color[3] == Approx(0.5f));
  CHECK(std::abs(out[0].clip[0]) == Approx(0.2f));
}

TEST_CASE("particles are laid out farthest first", "[render-fx][quads]") {
  FxParticlePool pool(3);
  addParticle(pool, {0.1f, 0, 0.2f}, {}, 0.0f);
  addParticle(pool, {0.2f, 0, 0.9f}, {}, 0.0f);
  addParticle(pool, {0.3f, 0, 0.5f}, {}, 0.0f);
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVertex> out;

  buildFxQuads(pool, identityView(), order, out);
  REQUIRE(out.size() == 3 * FX_VERTICES_PER_PARTICLE);
  CHECK(out[0].clip[2] == 0.9f);
  CHECK(out[FX_VERTICES_PER_PARTICLE].clip[2] == 0.5f);
  CHECK(out[2 * FX_VERTICES_PER_PARTICLE].clip[2] == 0.2f);
}

TEST_CASE("a streaking particle is drawn out along its motion on screen",
          "[render-fx][quads]") {
  FxParticlePool pool(1);
  // A tile a second is a hundred pixels a second; half a second of it is a
  // fifty-pixel streak, twenty-five either side, on top of its radius.
  addParticle(pool, {}, {1.0f, 0.0f, 0.0f}, 0.5f);
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVertex> out;

  buildFxQuads(pool, identityView(), order, out);
  REQUIRE(out.size() == FX_VERTICES_PER_PARTICLE);
  for (const FxVertex& v : out) {
    CHECK(std::abs(v.clip[0]) == Approx(0.35f));
    CHECK(std::abs(v.clip[1]) == Approx(0.1f));
  }
}

TEST_CASE("a surface with no size lays nothing out", "[render-fx][quads]") {
  FxParticlePool pool(1);
  addParticle(pool, {}, {}, 0.0f);
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVertex> out(3);

  buildFxQuads(pool, {Mat4::identity(), 0.0f, 100.0f}, order, out);
  CHECK(out.empty());
}

TEST_CASE("a puff carries its shape and its own seed to the shader",
          "[render-fx][quads]") {
  FxParticlePool pool(2);
  addParticle(pool, {}, {}, 0.0f);
  pool.look[0].shape = FxParticleShape::PUFF;
  pool.angle[0] = 123.0f;
  addParticle(pool, {1.0f, 0.0f, 0.0f}, {}, 0.0f);
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVertex> out;

  buildFxQuads(pool, identityView(), order, out);
  REQUIRE(out.size() == 2 * FX_VERTICES_PER_PARTICLE);
  const FxVertex& puff = out[0].shape[0] == 1.0f ? out[0] : out[6];
  const FxVertex& disc = out[0].shape[0] == 1.0f ? out[6] : out[0];
  REQUIRE(puff.shape[0] == 1.0f);
  REQUIRE(puff.shape[1] == 123.0f);
  REQUIRE(disc.shape[0] == 0.0f);
}

TEST_CASE("a particle is turned by its angle, and its spin turns it on",
          "[render-fx][quads]") {
  FxParticlePool pool(1);
  addParticle(pool, {}, {}, 0.0f);
  // An eighth of a turn puts the corner that was off the quad's bottom
  // right straight out to its right, at the diagonal's length.
  pool.angle[0] = 45.0f;
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVertex> out;
  buildFxQuads(pool, identityView(), order, out);
  const float diagonal = 0.1f * std::numbers::sqrt2_v<float>;
  REQUIRE(out[1].clip[0] == Approx(diagonal).margin(1e-5));
  REQUIRE(out[1].clip[1] == Approx(0.0f).margin(1e-5));

  // A second of 45 degrees a second leaves it exactly where that did.
  pool.angle[0] = 0.0f;
  pool.look[0].spin = 45.0f;
  pool.age[0] = 1.0f;
  buildFxQuads(pool, identityView(), order, out);
  REQUIRE(out[1].clip[0] == Approx(diagonal).margin(1e-5));
}

/// One white-warm point light standing at the origin, reaching ten tiles.
const std::array<MeshLight, 1> ONE_LIGHT{MeshLight{
    {0.0f, 0.0f, 0.0f}, 10.0f, {}, 1.0f, {1.0f, 0.5f, 0.0f}, MESH_LIGHT_POINT}};

TEST_CASE("a lit particle takes the scene's light; an emissive one does not",
          "[render-fx][quads]") {
  FxParticlePool pool(1);
  addParticle(pool, {}, {}, 0.0f);
  pool.look[0].lighting = FxParticleLighting::LIT;
  pool.look[0].color_start = {1.0f, 1.0f, 1.0f, 0.5f};
  pool.look[0].color_end = pool.look[0].color_start;
  FxQuadView view = identityView();
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVertex> out;

  // Unlit scene: only the ambient floor reaches it, and what it hides is
  // its own whatever the light does.
  buildFxQuads(pool, view, order, out);
  REQUIRE(out[0].color[0] == Approx(MESH_LIGHT_AMBIENT));
  REQUIRE(out[0].color[3] == Approx(0.5f));

  view.lights = ONE_LIGHT;
  buildFxQuads(pool, view, order, out);
  REQUIRE(out[0].color[0] == Approx(MESH_LIGHT_AMBIENT + MESH_LIGHT_DIFFUSE));
  REQUIRE(out[0].color[1] ==
          Approx(MESH_LIGHT_AMBIENT + MESH_LIGHT_DIFFUSE * 0.5f));
}

TEST_CASE("an emissive particle keeps its own colour under any light",
          "[render-fx][quads]") {
  FxParticlePool pool(1);
  addParticle(pool, {}, {}, 0.0f);
  pool.look[0].color_start = {1.0f, 1.0f, 1.0f, 0.5f};
  pool.look[0].color_end = pool.look[0].color_start;
  FxQuadView view = identityView();
  view.lights = ONE_LIGHT;
  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVertex> out;

  buildFxQuads(pool, view, order, out);
  REQUIRE(out[0].color[0] == Approx(1.0f));
}
