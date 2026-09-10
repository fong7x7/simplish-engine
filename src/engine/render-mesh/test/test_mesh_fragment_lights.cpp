#include <catch2/catch_test_macros.hpp>
#include <engine/render-mesh/mesh-fragment-lights.h>
#include <vector>

using eng::makeMeshFragmentLights;
using eng::MESH_MAX_LIGHTS;
using eng::MeshFragmentLights;
using eng::MeshLight;

TEST_CASE("no lights becomes the one built-in key light") {
  const MeshFragmentLights block = makeMeshFragmentLights({}, 3);
  REQUIRE(block.count == 1);
  REQUIRE(block.shade_bands == 3);
  REQUIRE(block.lights[0].intensity == MeshLight{}.intensity);
}

TEST_CASE("the lights given are copied in order") {
  std::vector<MeshLight> lights(2);
  lights[1].intensity = 0.25f;
  const MeshFragmentLights block = makeMeshFragmentLights(lights, 0);
  REQUIRE(block.count == 2);
  REQUIRE(block.lights[1].intensity == 0.25f);
}

TEST_CASE("lights past the shader's array are dropped") {
  const std::vector<MeshLight> lights(MESH_MAX_LIGHTS + 3);
  REQUIRE(makeMeshFragmentLights(lights, 0).count == MESH_MAX_LIGHTS);
}
