#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-footstep-choices.h>

using namespace eng;
using namespace eng::editor;

TEST_CASE("the Surface row starts with the ground's, then every surface") {
  const std::vector<std::string> names = editorSurfaceChoiceNames();
  REQUIRE(names.size() == game::FOOTSTEP_SURFACE_COUNT + 1);
  REQUIRE(names.front() == "From the ground");
  REQUIRE_FALSE(editorSurfaceChoice(0).has_value());
  REQUIRE(editorSurfaceChoiceIndex(std::nullopt) == 0);
}

TEST_CASE("every surface is picked back from its own place in the row") {
  for (const game::FootstepSurface surface : game::ALL_FOOTSTEP_SURFACES) {
    REQUIRE(editorSurfaceChoice(editorSurfaceChoiceIndex(surface)) == surface);
  }
}

TEST_CASE("the Footsteps row lists every step set") {
  REQUIRE(editorFootstepChoiceNames().size() == game::STEP_SET_COUNT);
  REQUIRE(editorFootstepChoiceNames()[1] == "Boots");
}
