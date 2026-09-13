#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-emitter-ops.h>
#include <editor/shell/editor-emitter-player.h>
#include <vector>

using namespace eng;
using namespace eng::editor;

namespace {

/// An emitter throwing 3 long-lived particles every @p interval seconds,
/// with a flash each time.
EditorEmitter emitterEvery(float interval) {
  EditorEmitter emitter = makeEditorEmitter("wall_sparks", {});
  emitter.interval = interval;
  emitter.burst.count = 3;
  emitter.burst.life_min = 100.0f;
  emitter.burst.life_max = 100.0f;
  return emitter;
}

}  // namespace

TEST_CASE("an emitter bursts the moment it is first seen, then on its "
          "interval") {
  const std::vector<EditorEmitter> emitters{emitterEvery(0.5f)};
  EditorEmitterPlayer player;
  FxWorld world(1);

  player.advance(emitters, 0.01f, world);
  REQUIRE(world.particles.live == 3);
  REQUIRE(world.lights.live == 1);
  player.advance(emitters, 0.4f, world);
  REQUIRE(world.particles.live == 3);
  player.advance(emitters, 0.2f, world);
  REQUIRE(world.particles.live == 6);
}

TEST_CASE("a long frame throws a few bursts at most, and owes no more") {
  const std::vector<EditorEmitter> emitters{emitterEvery(0.1f)};
  EditorEmitterPlayer player;
  FxWorld world(1);

  player.advance(emitters, 60.0f, world);
  REQUIRE(world.particles.live == 3 * EDITOR_EMITTER_MAX_BURSTS_PER_STEP);
  // The backlog is let go: the next burst is a whole interval away.
  player.advance(emitters, 0.05f, world);
  REQUIRE(world.particles.live == 3 * EDITOR_EMITTER_MAX_BURSTS_PER_STEP);
}

TEST_CASE("an interval of zero is a stream, not a hang") {
  const std::vector<EditorEmitter> emitters{emitterEvery(0.0f)};
  EditorEmitterPlayer player;
  FxWorld world(1);

  player.advance(emitters, 0.05f, world);
  // One at once, and one each `EDITOR_EMITTER_MIN_INTERVAL` after it.
  REQUIRE(world.particles.live == 3 * 3);
}

TEST_CASE("an emitter added later bursts when it is seen; reset starts over") {
  std::vector<EditorEmitter> emitters{emitterEvery(10.0f)};
  EditorEmitterPlayer player;
  FxWorld world(1);
  player.advance(emitters, 0.01f, world);
  REQUIRE(world.particles.live == 3);

  emitters.push_back(emitterEvery(10.0f));
  player.advance(emitters, 0.01f, world);
  REQUIRE(world.particles.live == 6);

  player.reset();
  player.advance(emitters, 0.01f, world);
  REQUIRE(world.particles.live == 12);
}

TEST_CASE("the player counts each emitter's bursts, and reset starts the "
          "count over") {
  const std::vector<EditorEmitter> emitters{emitterEvery(0.5f),
                                            emitterEvery(10.0f)};
  EditorEmitterPlayer player;
  FxWorld world(1);
  player.advance(emitters, 0.01f, world);
  player.advance(emitters, 1.0f, world);

  REQUIRE(player.bursts().size() == 2);
  REQUIRE(player.bursts()[0] == 3);
  REQUIRE(player.bursts()[1] == 1);
  player.reset();
  REQUIRE(player.bursts().empty());
}
