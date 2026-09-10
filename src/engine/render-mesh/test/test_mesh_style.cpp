#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/render-mesh/mesh-style.h>

using Catch::Approx;
using namespace eng;

TEST_CASE("smooth shading leaves a light's strength alone") {
  REQUIRE(meshShadeBand(0.37f, MESH_SHADE_SMOOTH) == Approx(0.37f));
  REQUIRE(meshShadeBand(1.0f, MESH_SHADE_SMOOTH) == Approx(1.0f));
}

TEST_CASE("one band is the same as none") {
  // A single tone would be no light at all, which nobody means by asking
  // for one band; it reads as smooth instead.
  REQUIRE(meshShadeBand(0.37f, 1) == Approx(0.37f));
}

TEST_CASE("three bands give an unlit, a half-lit and a fully lit tone") {
  REQUIRE(meshShadeBand(0.0f, 3) == Approx(0.0f));
  REQUIRE(meshShadeBand(0.32f, 3) == Approx(0.0f));
  REQUIRE(meshShadeBand(0.34f, 3) == Approx(0.5f));
  REQUIRE(meshShadeBand(0.65f, 3) == Approx(0.5f));
  REQUIRE(meshShadeBand(0.67f, 3) == Approx(1.0f));
  // Full strength lands in the top band rather than one past it.
  REQUIRE(meshShadeBand(1.0f, 3) == Approx(1.0f));
}

namespace {

/// Steps a sweep from no light to full takes.
constexpr int SWEEP_STEPS = 100;

/// The light at step @p i of that sweep.
float sweepLight(int i) {
  return static_cast<float>(i) / static_cast<float>(SWEEP_STEPS);
}

}  // namespace

TEST_CASE("banding never brightens past full or darkens below none") {
  for (uint32_t bands : {2u, 3u, 4u, 8u}) {
    for (int i = 0; i <= SWEEP_STEPS; ++i) {
      const float banded = meshShadeBand(sweepLight(i), bands);
      REQUIRE(banded >= 0.0f);
      REQUIRE(banded <= 1.0f);
    }
  }
}

TEST_CASE("banding keeps the order of what it bands") {
  // A brighter surface never comes out darker than a dimmer one, which is
  // what keeps a banded form reading as lit from the same side.
  float previous = 0.0f;
  for (int i = 0; i <= SWEEP_STEPS; ++i) {
    const float banded = meshShadeBand(sweepLight(i), 4);
    REQUIRE(banded >= previous);
    previous = banded;
  }
}

TEST_CASE("a light that does not reach a surface stays dark when banded") {
  REQUIRE(meshShadeBand(-0.2f, 3) == Approx(0.0f));
}

TEST_CASE("the smooth style is how meshes always drew") {
  REQUIRE(MESH_STYLE_SMOOTH.shade_bands == MESH_SHADE_SMOOTH);
  REQUIRE(MESH_STYLE_SMOOTH.outline_width == 0.0f);
}

TEST_CASE("the cel style bands its light and draws a line") {
  REQUIRE(MESH_STYLE_CEL.shade_bands >= 2);
  REQUIRE(MESH_STYLE_CEL.outline_width > 0.0f);
}
