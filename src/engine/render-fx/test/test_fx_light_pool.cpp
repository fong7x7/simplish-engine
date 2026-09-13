#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/render-fx/fx-light-pool.h>

using Catch::Approx;
using namespace eng;

namespace {

/// A warm flash of @p intensity, reaching three tiles, lasting @p life.
FxFlash testFlash(float intensity, float life) {
  return {{1.0f, 0.8f, 0.5f}, intensity, 3.0f, life};
}

}  // namespace

TEST_CASE("a flash with no intensity, range or life is no flash",
          "[render-fx][lights]") {
  FxLightPool pool(4);
  emitFxFlash(pool, testFlash(0.0f, 1.0f), {});
  emitFxFlash(pool, testFlash(1.0f, 0.0f), {});
  emitFxFlash(pool, {{1, 1, 1}, 1.0f, 0.0f, 1.0f}, {});
  CHECK(pool.live == 0);
}

TEST_CASE("a flash lights as a point light, fading with its life squared",
          "[render-fx][lights]") {
  FxLightPool pool(4);
  emitFxFlash(pool, testFlash(2.0f, 1.0f), {1.0f, 2.0f, 0.9f});

  MeshLight light = fxLightAt(pool, 0);
  CHECK(light.kind == MESH_LIGHT_POINT);
  CHECK(light.position.y == 2.0f);
  CHECK(light.range == 3.0f);
  CHECK(light.color.y == 0.8f);
  CHECK(light.intensity == Approx(2.0f));

  stepFxLights(pool, 0.5f);
  light = fxLightAt(pool, 0);
  CHECK(light.intensity == Approx(0.5f));

  stepFxLights(pool, 0.5f);
  CHECK(pool.live == 0);
}

TEST_CASE("a full pool gives a new flash the place of the most faded",
          "[render-fx][lights]") {
  FxLightPool pool(2);
  emitFxFlash(pool, testFlash(1.0f, 1.0f), {1, 0, 0});
  emitFxFlash(pool, testFlash(1.0f, 0.2f), {2, 0, 0});
  stepFxLights(pool, 0.1f);

  // The second is halfway through its life, the first a tenth.
  emitFxFlash(pool, testFlash(3.0f, 1.0f), {3, 0, 0});
  REQUIRE(pool.live == 2);
  CHECK(pool.position[0].x == 1.0f);
  CHECK(pool.position[1].x == 3.0f);
  CHECK(pool.age[1] == 0.0f);
}

TEST_CASE("the brightest flashes are appended after what is there already",
          "[render-fx][lights]") {
  FxLightPool pool(8);
  emitFxFlash(pool, testFlash(1.0f, 1.0f), {1, 0, 0});
  emitFxFlash(pool, testFlash(4.0f, 1.0f), {2, 0, 0});
  emitFxFlash(pool, testFlash(2.0f, 1.0f), {3, 0, 0});
  std::vector<MeshLight> lights(1);

  appendBrightestFxLights(pool, 2, lights);
  REQUIRE(lights.size() == 3);
  // The placed light is untouched, then the brightest two, brightest first.
  CHECK(lights[0].kind == MESH_LIGHT_DIRECTIONAL);
  CHECK(lights[1].position.x == 2.0f);
  CHECK(lights[2].position.x == 3.0f);

  appendBrightestFxLights(pool, 0, lights);
  CHECK(lights.size() == 3);
  appendBrightestFxLights(pool, 10, lights);
  CHECK(lights.size() == 6);

  clearFxLights(pool);
  CHECK(pool.live == 0);
}
