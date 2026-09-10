#include <catch2/catch_test_macros.hpp>
#include <engine/sim/replay-recorder.h>
#include <engine/sim/tick-hash-builder.h>

using eng::sim::Replay;
using eng::sim::ReplayRecorder;
using eng::sim::TickHashBuilder;
using eng::sim::TickInput;
using eng::sim::TickResult;

namespace {

TickResult hashedResult(uint64_t tick) {
  TickHashBuilder builder;
  builder.section("tick").add(tick);
  return TickResult{tick, builder.finish(tick)};
}

}  // namespace

TEST_CASE("ReplayRecorder keeps every input and the header") {
  ReplayRecorder recorder({"main", 1, 2, 1}, 10);
  for (uint64_t tick = 0; tick < 5; ++tick) {
    TickInput input;
    input.players[0].buttons = static_cast<uint32_t>(tick);
    recorder.record(input, hashedResult(tick));
  }
  const Replay replay = recorder.finish();
  CHECK(replay.header.level_id == "main");
  REQUIRE(replay.inputs.size() == 5);
  CHECK(replay.inputs[4].players[0].buttons == 4);
}

TEST_CASE("ReplayRecorder checkpoints on the interval and at the end") {
  ReplayRecorder recorder({}, 10);
  for (uint64_t tick = 0; tick < 25; ++tick) {
    recorder.record(TickInput{}, hashedResult(tick));
  }
  const Replay replay = recorder.finish();
  REQUIRE(replay.checkpoints.size() == 4);
  CHECK(replay.checkpoints[0].tick == 0);
  CHECK(replay.checkpoints[1].tick == 10);
  CHECK(replay.checkpoints[2].tick == 20);
  CHECK(replay.checkpoints[3].tick == 24);
}

TEST_CASE("ReplayRecorder does not repeat a final tick on the interval") {
  ReplayRecorder recorder({}, 10);
  for (uint64_t tick = 0; tick <= 20; ++tick) {
    recorder.record(TickInput{}, hashedResult(tick));
  }
  const Replay replay = recorder.finish();
  REQUIRE(replay.checkpoints.size() == 3);
  CHECK(replay.checkpoints.back().tick == 20);
}

TEST_CASE("ReplayRecorder without hashes records inputs and no checkpoints") {
  ReplayRecorder recorder({}, 10);
  for (uint64_t tick = 0; tick < 12; ++tick) {
    recorder.record(TickInput{}, TickResult{tick, std::nullopt});
  }
  const Replay replay = recorder.finish();
  CHECK(replay.inputs.size() == 12);
  CHECK(replay.checkpoints.empty());
}
