#include <catch2/catch_test_macros.hpp>
#include <game/content/footstep-names.h>

using namespace eng::game;

TEST_CASE("every step set is read back from its own word") {
  for (const StepSet steps : ALL_STEP_SETS) {
    REQUIRE(stepSetNamed(stepSetWord(steps)) == steps);
    REQUIRE_FALSE(stepSetLabel(steps).empty());
  }
  REQUIRE_FALSE(stepSetNamed("hooves").has_value());
}

TEST_CASE("every surface is read back from its own word") {
  for (const FootstepSurface surface : ALL_FOOTSTEP_SURFACES) {
    REQUIRE(footstepSurfaceNamed(footstepSurfaceWord(surface)) == surface);
    REQUIRE_FALSE(footstepSurfaceLabel(surface).empty());
  }
  REQUIRE_FALSE(footstepSurfaceNamed("lava").has_value());
}
