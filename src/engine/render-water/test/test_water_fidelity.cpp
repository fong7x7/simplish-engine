#include <catch2/catch_test_macros.hpp>
#include <engine/render-water/water-fidelity.h>

using namespace eng;

TEST_CASE("every fidelity is named by a word that names it back",
          "[render-water][fidelity]") {
  for (const WaterFidelity fidelity : WATER_FIDELITIES) {
    CHECK(waterFidelityNamed(waterFidelityWord(fidelity)) == fidelity);
  }
  CHECK(waterFidelityWord(WaterFidelity::FLAT) == "flat");
  CHECK(waterFidelityWord(WaterFidelity::HIGH) == "high");
  CHECK_FALSE(waterFidelityNamed("ultra").has_value());
}

TEST_CASE("flat water simulates nothing and high simulates finest",
          "[render-water][fidelity]") {
  CHECK_FALSE(waterFidelitySimulates(WaterFidelity::FLAT));
  CHECK(waterFidelitySimulates(WaterFidelity::LOW));
  CHECK(waterSamplesPerTile(WaterFidelity::FLAT) ==
        WATER_FLAT_SAMPLES_PER_TILE);
  CHECK(waterSamplesPerTile(WaterFidelity::FLAT) <
        waterSamplesPerTile(WaterFidelity::LOW));
  CHECK(waterSamplesPerTile(WaterFidelity::LOW) <
        waterSamplesPerTile(WaterFidelity::HIGH));
  CHECK(WATER_DEFAULT_FIDELITY == WaterFidelity::HIGH);
}
