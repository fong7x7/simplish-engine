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

/// Which sample of @p field @p at lies in.
size_t slotNear(const WaterField& field, Vec2 at) {
  const auto n = static_cast<float>(field.samples_per_tile);
  const auto x =
      static_cast<size_t>((at.x - static_cast<float>(field.origin.x)) * n);
  const auto y =
      static_cast<size_t>((at.y - static_cast<float>(field.origin.y)) * n);
  return y * field.width + x;
}

/// Step @p field @p frames frames of the fixed step.
void stepFrames(WaterField& field, int frames) {
  for (int frame = 0; frame < frames; ++frame) {
    stepWaterField(field, WATER_STEP_SECONDS);
  }
}

/// A river 16 tiles long and 6 wide, a tile deep, running east as fast as
/// water runs.
WaterLayer river() {
  WaterLayer layer;
  for (int32_t y = 0; y < 6; ++y) {
    for (int32_t x = 0; x < 16; ++x) {
      setWaterCell(layer, {x, y},
                   {.depth = waterDepthUnits(1.0f), .flow_speed = 255});
    }
  }
  return layer;
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

/// A pond of fluid @p viscosity thick, 255 the thickest, pushed at its
/// middle and aged @p seconds.
WaterField pushedThick(uint8_t viscosity, float seconds) {
  WaterLayer layer;
  for (int32_t y = 0; y < 16; ++y) {
    for (int32_t x = 0; x < 16; ++x) {
      setWaterCell(layer, {x, y}, {.depth = 16, .viscosity = viscosity});
    }
  }
  WaterField field = fieldOver(layer, 8);
  disturbWaterField(field, {8.0f, 8.0f}, 0.4f, 0.05f);
  for (float t = 0.0f; t < seconds; t += WATER_STEP_SECONDS) {
    stepWaterField(field, WATER_STEP_SECONDS);
  }
  return field;
}

// Req: docs/engine/water.md §3 — a thick fluid carries a ripple slower than
// water: a second in, water's ring (2.5 tiles a second) has passed 1.8 tiles
// out, and the thickest fluid's (1.0) has not reached it.
TEST_CASE("a ripple crawls through thick fluid", "[render-water][field]") {
  const WaterField water = pushedThick(0, 0.6f);
  const WaterField thick = pushedThick(255, 0.6f);
  REQUIRE(thick.viscous);
  CHECK_FALSE(water.viscous);
  CHECK(std::abs(levelNear(water, {9.8f, 8.0f})) >
        10.0f * std::abs(levelNear(thick, {9.8f, 8.0f})));
}

// Req: docs/engine/water.md §3 — thick fluid does not ring: a dent in it
// settles slowly back rather than rippling out, so it moves far less.
TEST_CASE("thick fluid moves far less than water once pushed",
          "[render-water][field]") {
  const auto motion = [](const WaterField& field) {
    double sum = 0.0;
    for (const float v : field.velocity) {
      sum += static_cast<double>(v) * v;
    }
    return sum;
  };
  const double water = motion(pushedThick(0, 1.0f));
  CHECK(motion(pushedThick(255, 1.0f)) < 0.2 * water);
  CHECK(motion(pushedThick(128, 1.0f)) < water);
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

namespace {

/// The median of @p field's frame times, printed under @p name with the
/// worst: the first few frames warm the caches and are not counted.
double medianFrameMs(WaterField& field, const char* name) {
  std::vector<double> times = frameTimes(field, 250);
  times.erase(times.begin(), times.begin() + 10);
  std::sort(times.begin(), times.end());
  const double median = times[times.size() / 2];
  std::printf("%s: %u samples, median %.3f ms, worst %.3f ms\n", name,
              static_cast<unsigned>(field.level.size()), median, times.back());
  return median;
}

}  // namespace

TEST_CASE("the largest field steps and packs inside its budget", "[.][perf]") {
  // 62 × 62 tiles of water and the dry ring round them, at eight samples a
  // tile, is exactly `WATER_MAX_SAMPLES`: the biggest pond HIGH simulates
  // at its full resolution.
  WaterField field = fieldOver(pond(62, 62), 8);
  REQUIRE(field.samples_per_tile == 8);
  CHECK(medianFrameMs(field, "water") <= WATER_FRAME_BUDGET_MS);
}

TEST_CASE("the largest flowing field steps and packs inside its budget",
          "[.][perf]") {
  // The widest river simulated at eight samples a tile: 43 × 43 tiles and
  // the dry ring round them fit `WATER_MAX_FLOWING_SAMPLES`. Every step
  // carries its level, its speed and its foam downstream too.
  WaterLayer layer;
  for (int32_t y = 0; y < 43; ++y) {
    for (int32_t x = 0; x < 43; ++x) {
      setWaterCell(
          layer, {x, y},
          {.depth = waterDepthUnits(WATER_DEFAULT_DEPTH), .flow_speed = 255});
    }
  }
  WaterField field = fieldOver(layer, 8);
  REQUIRE(field.flowing);
  REQUIRE(field.samples_per_tile == 8);
  CHECK(medianFrameMs(field, "flowing water") <= WATER_FRAME_BUDGET_MS);
}

// Req: docs/engine/water.md §3 — something standing in the water is a shore
// of its own: dry under its footprint, the shore distance measured to it,
// and the ripples stopped at it.
TEST_CASE("the water is dry under what stands in it, and its shore runs round",
          "[render-water][field]") {
  const WaterObstacle stone{{5.0f, 5.0f}, {7.0f, 7.0f}};
  WaterField field;
  resetWaterField(field, pond(12, 12), 8, std::span(&stone, 1));
  CHECK_FALSE(waterFieldWetAt(field, {6.0f, 6.0f}));
  CHECK(waterFieldWetAt(field, {4.9f, 6.0f}));
  CHECK(field.wet_count == 12U * 12U * 64U - 16U * 16U);
  CHECK(field.shore[slotNear(field, {4.9f, 6.0f})] == Approx(0.125f));
  REQUIRE(disturbWaterField(field, {3.5f, 6.0f}, 0.6f, 0.05f));
  stepWaterField(field, 1.0f);
  CHECK(dryIsStill(field));
}

// Req: docs/engine/water.md §3 — churned water foams, and the foam lingers
// and thins rather than vanishing with the ripple that made it; drizzle
// throws none.
TEST_CASE("a hard push leaves foam that lingers and thins",
          "[render-water][field]") {
  WaterField field = fieldOver(pond(8, 8), 8);
  const size_t middle = slotNear(field, {4.0f, 4.0f});
  REQUIRE(disturbWaterField(field, {4.0f, 4.0f}, 0.8f, 0.2f));
  CHECK(field.foam[middle] > 0.9f);
  stepFrames(field, 4);
  CHECK(field.foam[middle] > 0.5f);
  stepFrames(field, 300);
  CHECK(field.foam[middle] < 0.1f);
  WaterField calm = fieldOver(pond(8, 8), 8);
  REQUIRE(disturbWaterField(calm, {4.0f, 4.0f}, 0.2f, 0.01f));
  CHECK(calm.foam == std::vector<float>(calm.foam.size(), 0.0f));
}

// Req: docs/engine/water.md §3 — flowing water carries what is on it
// downstream: foam thrown in a river moves the way the river runs, and the
// flow is slowed to nothing at the bank.
TEST_CASE("a river flows its whole speed mid-stream, and less at the bank",
          "[render-water][field]") {
  const WaterField field = fieldOver(river(), 8);
  REQUIRE(field.flowing);
  const size_t middle = slotNear(field, {8.0f, 3.0f});
  CHECK(field.flow_x[middle] == Approx(WATER_MAX_FLOW_SPEED));
  CHECK(field.flow_y[middle] == Approx(0.0f).margin(1e-5f));
  CHECK(field.flow_x[slotNear(field, {8.0f, 0.05f})] < 0.5f);
}

TEST_CASE("flowing water carries its foam downstream",
          "[render-water][field]") {
  WaterField field = fieldOver(river(), 8);
  REQUIRE(disturbWaterField(field, {4.0f, 3.0f}, 0.4f, 0.2f));
  stepFrames(field, 60);
  const float downstream = field.foam[slotNear(field, {5.5f, 3.0f})];
  CHECK(downstream > field.foam[slotNear(field, {4.0f, 3.0f})]);
  CHECK(downstream > 0.1f);
}

TEST_CASE("still water does not flow", "[render-water][field]") {
  CHECK_FALSE(fieldOver(pond(4, 4), 8).flowing);
}

// Req: docs/engine/water.md §7 — flowing water costs more a sample, so it
// has half the budget: a river as wide as the widest lake is simulated a
// step coarser.
TEST_CASE("a wide river is simulated coarser than a lake as wide",
          "[render-water][field]") {
  WaterLayer lake = pond(62, 62);
  WaterLayer river = lake;
  setWaterCell(river, {0, 0}, {.depth = 16, .flow_speed = 40});
  CHECK(fieldOver(lake, 8).samples_per_tile == 8);
  CHECK(fieldOver(river, 8).samples_per_tile == 4);
}
