#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <engine/render-water/water-field.h>
#include <engine/render-water/water-texels.h>

using Catch::Approx;
using namespace eng;

namespace {


/// What stepping the field and packing its texels may take a frame, in
/// milliseconds, at `WATER_MAX_SAMPLES`: water's share of Engine §7's 3 ms
/// for render submission (docs/engine/water.md §6).
constexpr double WATER_FRAME_BUDGET_MS = 1.0;

/// A pond @p width × @p height cells with its south-west cell at the
/// origin, every cell @p depth tiles deep.
WaterLayer pond(int32_t width, int32_t height,
                float depth = WATER_DEFAULT_DEPTH) {
  WaterLayer layer;
  for (int32_t y = 0; y < height; ++y) {
    for (int32_t x = 0; x < width; ++x) {
      setWaterCell(layer, {x, y}, {.depth = waterDepthUnits(depth)});
    }
  }
  return layer;
}

/// A field over @p layer's water at @p samples_per_tile.
WaterField fieldOver(const WaterLayer& layer, uint32_t samples_per_tile) {
  WaterField field;
  resetWaterField(field, layer, samples_per_tile);
  return field;
}

/// The level of the sample nearest @p at.
float levelNear(const WaterField& field, Vec2 at) {
  const auto n = static_cast<float>(field.samples_per_tile);
  const auto x =
      static_cast<size_t>((at.x - static_cast<float>(field.origin.x)) * n);
  const auto y =
      static_cast<size_t>((at.y - static_cast<float>(field.origin.y)) * n);
  return field.level[y * field.width + x];
}

/// Whether every dry sample is still exactly at rest.
bool dryIsStill(const WaterField& field) {
  for (size_t i = 0; i < field.level.size(); ++i) {
    if (field.keep[i] == 0.0f && field.level[i] != 0.0f) {
      return false;
    }
  }
  return true;
}

/// How long each of @p frames frames takes to step @p field and pack its
/// texels, in milliseconds.
std::vector<double> frameTimes(WaterField& field, int frames) {
  using Clock = std::chrono::steady_clock;
  std::vector<uint8_t> texels;
  std::vector<double> times;
  for (int frame = 0; frame < frames; ++frame) {
    const auto start = Clock::now();
    stepWaterField(field, WATER_STEP_SECONDS);
    writeWaterTexels(field, texels);
    times.push_back(
        std::chrono::duration<double, std::milli>(Clock::now() - start)
            .count());
  }
  return times;
}

}  // namespace

TEST_CASE("a field over no water is empty", "[render-water][field]") {
  const WaterField field = fieldOver(WaterLayer{}, 8);
  CHECK(waterFieldEmpty(field));
  CHECK(field.level.empty());

  WaterField none = fieldOver(pond(3, 3), 0);
  CHECK(waterFieldEmpty(none));
}

TEST_CASE("a field covers its water and a tile of dry land round it",
          "[render-water][field]") {
  const WaterField field = fieldOver(pond(3, 2), 4);
  CHECK(field.origin == GroundCell{-1, -1});
  CHECK(field.samples_per_tile == 4);
  CHECK(field.width == 5 * 4);
  CHECK(field.height == 4 * 4);
  CHECK(field.wet_count == 3 * 2 * 16);
  CHECK(waterFieldWetAt(field, {0.1f, 0.1f}));
  CHECK(waterFieldWetAt(field, {2.9f, 1.9f}));
  CHECK_FALSE(waterFieldWetAt(field, {-0.1f, 0.5f}));
  CHECK_FALSE(waterFieldWetAt(field, {3.1f, 0.5f}));
  CHECK_FALSE(waterFieldWetAt(field, {40.0f, 40.0f}));
}

TEST_CASE("the shore distance grows into open water and stops counting",
          "[render-water][field]") {
  const WaterField field = fieldOver(pond(12, 12), 8);
  const auto shoreAt = [&](Vec2 at) {
    const auto x =
        static_cast<size_t>((at.x - static_cast<float>(field.origin.x)) * 8.0f);
    const auto y =
        static_cast<size_t>((at.y - static_cast<float>(field.origin.y)) * 8.0f);
    return field.shore[y * field.width + x];
  };
  CHECK(shoreAt({-0.5f, 5.0f}) == 0.0f);
  CHECK(shoreAt({0.05f, 5.0f}) == Approx(0.125f));
  CHECK(shoreAt({1.0f, 5.0f}) > shoreAt({0.2f, 5.0f}));
  CHECK(shoreAt({1.0f, 5.0f}) == Approx(1.0f).margin(0.13f));
  CHECK(shoreAt({6.0f, 6.0f}) == WATER_SHORE_TILES);
}

TEST_CASE("water too wide for the budget is simulated coarser, then not at all",
          "[render-water][field]") {
  // 102 × 102 tiles with the dry ring: 64 samples a tile would be 666k.
  const WaterField coarser = fieldOver(pond(100, 100), 8);
  CHECK(coarser.samples_per_tile == 4);
  CHECK(coarser.level.size() <= WATER_MAX_SAMPLES);

  WaterLayer sea;
  setWaterCell(sea, {0, 0}, {.depth = 16});
  setWaterCell(sea, {600, 600}, {.depth = 16});
  const WaterField none = fieldOver(sea, 8);
  CHECK(waterFieldEmpty(none));
}

/// A pond like `pond`, every cell @p depth tiles deep, at eight samples
/// a tile, pushed in its middle and aged @p seconds.
WaterField pushedPond(float depth, float seconds) {
  WaterField field = fieldOver(pond(16, 16, depth), 8);
  disturbWaterField(field, {8.0f, 8.0f}, 0.4f, 0.05f);
  for (float t = 0.0f; t < seconds; t += WATER_STEP_SECONDS) {
    stepWaterField(field, WATER_STEP_SECONDS);
  }
  return field;
}

TEST_CASE("a ripple runs faster across a lake than a puddle",
          "[render-water][field]") {
  const WaterField puddle = pushedPond(0.0625f, 0.5f);
  const WaterField lake = pushedPond(3.0f, 0.5f);
  // Half a second out, 2.5 tiles from the push: a lake's ring (4.3 tiles a
  // second) has passed there; a puddle's (0.6) has not reached it.
  CHECK(std::abs(levelNear(lake, {10.5f, 8.0f})) > 1e-4f);
  CHECK(std::abs(levelNear(puddle, {10.5f, 8.0f})) < 1e-5f);
}

TEST_CASE("a puddle stills sooner than a lake", "[render-water][field]") {
  const WaterField puddle = pushedPond(0.0625f, 2.0f);
  const WaterField lake = pushedPond(3.0f, 2.0f);
  CHECK(waterFieldEnergy(puddle) < waterFieldEnergy(lake));
}

TEST_CASE("each sample is as deep as its cells, shelving to the bank",
          "[render-water][field]") {
  const WaterField field = pushedPond(3.0f, 0.0f);
  const auto depthNear = [&](Vec2 at) {
    const auto x = static_cast<size_t>((at.x + 1.0f) * 8.0f);
    const auto y = static_cast<size_t>((at.y + 1.0f) * 8.0f);
    return field.depth[y * field.width + x];
  };
  CHECK(depthNear({8.0f, 8.0f}) == Approx(3.0f).margin(0.01f));
  CHECK(depthNear({0.1f, 8.0f}) < 0.2f);
  CHECK(depthNear({-0.5f, 8.0f}) == 0.0f);
}

TEST_CASE("a push over land does nothing and one over water moves it",
          "[render-water][field]") {
  WaterField field = fieldOver(pond(6, 6), 8);
  CHECK(waterFieldEnergy(field) == 0.0);
  CHECK_FALSE(disturbWaterField(field, {-0.5f, -0.5f}, 0.3f, 0.02f));
  CHECK(waterFieldEnergy(field) == 0.0);
  CHECK(disturbWaterField(field, {3.0f, 3.0f}, 0.4f, 0.02f));
  // The nearest sample's middle is a sixteenth of a tile off on each axis.
  CHECK(levelNear(field, {3.0f, 3.0f}) == Approx(-0.02f).margin(0.003f));
  CHECK(levelNear(field, {4.0f, 3.0f}) == 0.0f);
  CHECK(dryIsStill(field));
}

TEST_CASE("a ripple spreads outward and never onto land",
          "[render-water][field]") {
  WaterField field = fieldOver(pond(12, 12), 8);
  disturbWaterField(field, {6.0f, 6.0f}, 0.4f, 0.03f);
  CHECK(levelNear(field, {7.5f, 6.0f}) == 0.0f);
  for (int frame = 0; frame < 30; ++frame) {
    stepWaterField(field, WATER_STEP_SECONDS);
  }
  // 2.5 tiles a second for half a second: the ring is past a tile and a
  // half out, and moving the water it crosses.
  CHECK(std::abs(levelNear(field, {7.5f, 6.0f})) > 1e-4f);
  CHECK(dryIsStill(field));
}

TEST_CASE("ripples die away, and drizzle alone keeps the water barely moving",
          "[render-water][field]") {
  WaterField field = fieldOver(pond(8, 8), 8);
  disturbWaterField(field, {4.0f, 4.0f}, 0.6f, 0.05f);
  stepWaterField(field, WATER_STEP_SECONDS);
  const double pushed = waterFieldEnergy(field);
  for (int second = 0; second < 15 * 60; ++second) {
    stepWaterField(field, WATER_STEP_SECONDS);
  }
  const double settled = waterFieldEnergy(field);
  CHECK(settled < pushed * 0.1);
  CHECK(settled > 0.0);
}

TEST_CASE("the field stays bounded however hard it is pushed",
          "[render-water][field]") {
  for (const uint32_t samples : {1U, 2U, 4U, 8U}) {
    WaterField field = fieldOver(pond(10, 6), samples);
    for (int frame = 0; frame < 30 * 60; ++frame) {
      if (frame % 20 == 0) {
        disturbWaterField(field, {1.0f + static_cast<float>(frame % 7), 3.0f},
                          0.5f, 0.1f);
      }
      stepWaterField(field, WATER_STEP_SECONDS);
    }
    for (const float level : field.level) {
      REQUIRE(std::isfinite(level));
      REQUIRE(std::abs(level) < 0.5f);
    }
  }
}

TEST_CASE("a long frame runs no more steps than a frame is allowed",
          "[render-water][field]") {
  WaterField stalled = fieldOver(pond(5, 5), 8);
  WaterField capped = fieldOver(pond(5, 5), 8);
  disturbWaterField(stalled, {2.5f, 2.5f}, 0.4f, 0.03f);
  disturbWaterField(capped, {2.5f, 2.5f}, 0.4f, 0.03f);
  stepWaterField(stalled, 2.0f);
  for (uint32_t step = 0; step < WATER_MAX_STEPS_PER_FRAME; ++step) {
    stepWaterField(capped, WATER_STEP_SECONDS);
  }
  CHECK(stalled.level == capped.level);
  CHECK(stalled.pending_seconds < WATER_STEP_SECONDS);
}

TEST_CASE("the largest field steps and packs inside its budget", "[.][perf]") {
  // 62 × 62 tiles of water and the dry ring round them, at eight samples a
  // tile, is exactly `WATER_MAX_SAMPLES`: the biggest pond HIGH simulates
  // at its full resolution.
  WaterField field = fieldOver(pond(62, 62), 8);
  REQUIRE(field.samples_per_tile == 8);
  std::vector<double> times = frameTimes(field, 250);
  // The first few warm the caches and are not counted.
  times.erase(times.begin(), times.begin() + 10);
  std::sort(times.begin(), times.end());
  const double median = times[times.size() / 2];
  std::printf("water: %u samples, median %.3f ms, worst %.3f ms\n",
              static_cast<unsigned>(field.level.size()), median, times.back());
  CHECK(median <= WATER_FRAME_BUDGET_MS);
}
