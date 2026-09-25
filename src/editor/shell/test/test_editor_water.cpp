#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-menu-command.h>
#include <editor/shell/editor-water.h>
#include <engine/render-water/water-effect.h>

using namespace eng;
using namespace eng::editor;

namespace {

/// A pond of water @p side cells square, its south-west cell at the
/// origin.
WaterLayer pond(int32_t side) {
  WaterLayer layer;
  for (int32_t y = 0; y < side; ++y) {
    for (int32_t x = 0; x < side; ++x) {
      setWaterCell(layer, {x, y}, {.depth = 16});
    }
  }
  return layer;
}

/// Water shaped to an 8-cell pond at @p fidelity.
EditorWater waterAt(WaterFidelity fidelity) {
  EditorWater water;
  water.reshape(pond(8), fidelity);
  return water;
}

/// A cue of @p kind at (@p x, @p y).
game::CombatCue cueAt(game::CombatCueKind kind, float x, float y) {
  game::CombatCue cue{};
  cue.kind = kind;
  cue.at = {x, y, 0.5f};
  cue.radius = 2.0f;
  return cue;
}

/// The level of @p water's sample nearest @p at.
float levelAt(const EditorWater& water, Vec2 at) {
  const WaterField& field = water.field();
  const auto n = static_cast<float>(field.samples_per_tile);
  const auto x =
      static_cast<size_t>((at.x - static_cast<float>(field.origin.x)) * n);
  const auto y =
      static_cast<size_t>((at.y - static_cast<float>(field.origin.y)) * n);
  return field.level[y * field.width + x];
}

}  // namespace

TEST_CASE("water is reshaped when its ground or its fidelity changes") {
  EditorWater water;
  CHECK(water.reshape(pond(4), WaterFidelity::HIGH));
  const uint64_t shaped = water.shapeCount();
  CHECK_FALSE(water.reshape(pond(4), WaterFidelity::HIGH));
  CHECK(water.shapeCount() == shaped);
  CHECK(water.reshape(pond(4), WaterFidelity::LOW));
  CHECK(water.field().samples_per_tile == WATER_LOW_SAMPLES_PER_TILE);
  CHECK(water.reshape(pond(5), WaterFidelity::LOW));
  CHECK(water.shapeCount() == shaped + 2);
}

TEST_CASE("flat water is shaped, but nothing pushes it and it does not move") {
  EditorWater water = waterAt(WaterFidelity::FLAT);
  CHECK(water.field().samples_per_tile == WATER_FLAT_SAMPLES_PER_TILE);
  water.splash(std::vector{cueAt(game::CombatCueKind::BLAST, 4.0f, 4.0f)});
  water.wade(std::vector<Vec2>{{2.0f, 2.0f}});
  water.wade(std::vector<Vec2>{{2.5f, 2.0f}});
  water.advance(1.0f);
  EditorWaterState state;
  water.publish(state);
  CHECK(state.fidelity == WaterFidelity::FLAT);
  CHECK(state.wet_samples > 0);
  CHECK(state.pushes == 0);
  CHECK(state.energy == 0.0);
}

TEST_CASE("an empty level has no water to shape") {
  EditorWater water;
  water.reshape(WaterLayer{}, WaterFidelity::HIGH);
  CHECK(waterFieldEmpty(water.field()));
}

TEST_CASE("a wader pushes the water only when it moves, and only in it") {
  EditorWater water = waterAt(WaterFidelity::HIGH);
  water.wade(std::vector<Vec2>{{2.0f, 2.0f}, {-5.0f, -5.0f}});
  EditorWaterState state;
  water.publish(state);
  CHECK(state.pushes == 0);
  CHECK(state.energy == 0.0);
  // The first wades a step through the pond, pushing where it stands and
  // along its wake's two arms, three pushes each; the second, on dry land.
  water.wade(std::vector<Vec2>{{2.2f, 2.0f}, {-5.2f, -5.0f}});
  water.publish(state);
  CHECK(state.pushes == 7);
  CHECK(state.energy > 0.0);
  water.wade(std::vector<Vec2>{{2.2f, 2.0f}, {-5.2f, -5.0f}});
  water.publish(state);
  CHECK(state.pushes == 7);
}

// Req: docs/engine/water.md §6 — a wader leaves a wake opening behind it in
// a V: pushed along two arms trailing it, not beside it.
TEST_CASE("a wader's wake opens behind it") {
  EditorWater water = waterAt(WaterFidelity::HIGH);
  water.wade(std::vector<Vec2>{{4.0f, 4.0f}});
  water.wade(std::vector<Vec2>{{4.2f, 4.0f}});
  const float back = EDITOR_WAKE_TILES * 2.0f / 3.0f;
  const float arm = back * EDITOR_WAKE_SPREAD;
  CHECK(levelAt(water, {4.2f - back, 4.0f + arm}) < 0.0f);
  CHECK(levelAt(water, {4.2f - back, 4.0f - arm}) < 0.0f);
  CHECK(levelAt(water, {4.2f + back, 4.0f + arm}) == 0.0f);
}

// Req: docs/engine/water.md §6 — what lands in water splashes, at every
// fidelity, and a wader's feet splash every stride through it; nothing on
// land does.
TEST_CASE("hits in the water splash, at any fidelity, and hits on land not") {
  EditorWater water = waterAt(WaterFidelity::FLAT);
  water.splash(
      std::vector{cueAt(game::CombatCueKind::SHOT_HIT_WALL, 2.0f, 2.0f),
                  cueAt(game::CombatCueKind::BLAST, 4.0f, 4.0f),
                  cueAt(game::CombatCueKind::SHOT_HIT_BODY, -9.0f, 2.0f)});
  const std::vector<EditorWaterSplash> splashes = water.takeSplashes();
  REQUIRE(splashes.size() == 2);
  CHECK(splashes[1].scale == EDITOR_BLAST_SPLASH_SCALE);
  CHECK(water.takeSplashes().empty());
}

TEST_CASE("a wader's feet splash every stride through the water") {
  EditorWater water = waterAt(WaterFidelity::FLAT);
  water.wade(std::vector<Vec2>{{1.0f, 1.0f}});
  for (int step = 1; step <= 10; ++step) {
    water.wade(
        std::vector<Vec2>{{1.0f + 0.1f * static_cast<float>(step), 1.0f}});
  }
  const std::vector<EditorWaterSplash> splashes = water.takeSplashes();
  REQUIRE(splashes.size() == 1);
  CHECK(splashes[0].scale == EDITOR_WADE_SPLASH_SCALE);
}

TEST_CASE("a new crowd of waders is followed before it is pushed by") {
  EditorWater water = waterAt(WaterFidelity::HIGH);
  water.wade(std::vector<Vec2>{{2.0f, 2.0f}});
  water.wade(std::vector<Vec2>{{2.5f, 2.0f}, {3.0f, 3.0f}});
  water.forgetWaders();
  water.wade(std::vector<Vec2>{{6.0f, 6.0f}, {3.0f, 3.0f}});
  EditorWaterState state;
  water.publish(state);
  CHECK(state.pushes == 0);
}

TEST_CASE("hits and blasts in the water push it; a shot fired does not") {
  EditorWater water = waterAt(WaterFidelity::LOW);
  water.splash(std::vector{
      cueAt(game::CombatCueKind::SHOT_FIRED, 4.0f, 4.0f),
      cueAt(game::CombatCueKind::SHOT_HIT_WALL, 2.0f, 2.0f),
      cueAt(game::CombatCueKind::SHOT_HIT_BODY, 6.0f, 6.0f),
      cueAt(game::CombatCueKind::BLAST, 4.0f, 4.0f),
      cueAt(game::CombatCueKind::BLAST, -20.0f, -20.0f),
  });
  EditorWaterState state;
  water.publish(state);
  CHECK(state.pushes == 3);
}

TEST_CASE("ripples age on the clock the water is advanced by") {
  EditorWater water = waterAt(WaterFidelity::HIGH);
  water.splash(std::vector{cueAt(game::CombatCueKind::BLAST, 4.0f, 4.0f)});
  EditorWaterState pushed;
  water.publish(pushed);
  for (int frame = 0; frame < 600; ++frame) {
    water.advance(WATER_STEP_SECONDS);
  }
  EditorWaterState settled;
  water.publish(settled);
  CHECK(settled.energy < pushed.energy * 0.2);
  CHECK(water.seconds() > 9.0f);
  water.advance(0.0f);
  water.advance(-1.0f);
  CHECK(water.seconds() < 10.1f);
}

TEST_CASE("each water row of the View menu sets its own fidelity") {
  for (size_t i = 0; i < std::size(WATER_FIDELITIES); ++i) {
    CHECK(editorWaterFidelityOf(EDITOR_WATER_COMMANDS[i]) ==
          static_cast<int>(i));
  }
  CHECK(editorWaterFidelityOf(EditorMenuCommand::SET_SHADING_CEL) == -1);
}

// Req: docs/engine/water.md §6 — what stands in the water shapes it: moving
// a crate through a pond reshapes the ripples to go round it where it is.
TEST_CASE("water is reshaped round what stands in it when that moves") {
  EditorWater water;
  const WaterObstacle crate{{2.0f, 2.0f}, {3.0f, 3.0f}};
  CHECK(water.reshape(pond(6), WaterFidelity::HIGH, std::span(&crate, 1)));
  CHECK_FALSE(waterFieldWetAt(water.field(), {2.5f, 2.5f}));
  CHECK_FALSE(
      water.reshape(pond(6), WaterFidelity::HIGH, std::span(&crate, 1)));
  const WaterObstacle moved{{4.0f, 2.0f}, {5.0f, 3.0f}};
  CHECK(water.reshape(pond(6), WaterFidelity::HIGH, std::span(&moved, 1)));
  CHECK(waterFieldWetAt(water.field(), {2.5f, 2.5f}));
  CHECK_FALSE(waterFieldWetAt(water.field(), {4.5f, 2.5f}));
}

TEST_CASE("each water effect row of the View menu switches its own effect") {
  for (size_t i = 0; i < WATER_EFFECT_COUNT; ++i) {
    CHECK(editorWaterEffectOf(EDITOR_WATER_EFFECT_COMMANDS[i]) ==
          static_cast<int>(i));
  }
  CHECK(editorWaterEffectOf(EditorMenuCommand::SET_WATER_HIGH) == -1);
}
