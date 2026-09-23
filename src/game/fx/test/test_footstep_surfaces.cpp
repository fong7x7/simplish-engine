#include <catch2/catch_test_macros.hpp>
#include <game/fx/footstep-surfaces.h>

using namespace eng;
using namespace eng::game;

TEST_CASE("unpainted ground is ground") {
  const FootstepSurfaces level;
  REQUIRE(footstepSurfaceAt(level, {3.5F, -2.5F, 0}) ==
          FootstepSurface::GROUND);
}

TEST_CASE("a painted cell is the surface painted there") {
  FootstepSurfaces level;
  level.ground.set({-1, 2}, static_cast<uint8_t>(FootstepSurface::GRASS));
  REQUIRE(footstepSurfaceAt(level, {-0.5F, 2.9F, 0}) == FootstepSurface::GRASS);
  REQUIRE(footstepSurfaceAt(level, {0.5F, 2.9F, 0}) == FootstepSurface::GROUND);
}

TEST_CASE("a prop laid over the ground wins, the latest of two") {
  FootstepSurfaces level;
  level.ground.set({0, 0}, static_cast<uint8_t>(FootstepSurface::STONE));
  level.patches.push_back({{0, 0, 0}, {2, 2, 0.03F}, FootstepSurface::WOOD});
  level.patches.push_back(
      {{0.5F, 0.5F, 0}, {1, 1, 0.03F}, FootstepSurface::CLOTH});
  REQUIRE(footstepSurfaceAt(level, {0.2F, 0.2F, 0}) == FootstepSurface::WOOD);
  REQUIRE(footstepSurfaceAt(level, {0.7F, 0.7F, 0}) == FootstepSurface::CLOTH);
}

TEST_CASE("a platform overhead is not underfoot") {
  FootstepSurfaces level;
  level.patches.push_back({{0, 0, 3}, {2, 2, 3.2F}, FootstepSurface::METAL});
  REQUIRE(footstepSurfaceAt(level, {1, 1, 0}) == FootstepSurface::GROUND);
  REQUIRE(footstepSurfaceAt(level, {1, 1, 3.2F}) == FootstepSurface::METAL);
}
