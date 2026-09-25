#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <game/sdk/cooldown.h>
#include <game/sdk/every.h>
#include <game/sdk/phase.h>
#include <game/sdk/ticks.h>

using namespace eng::game::sdk;

static_assert(seconds(2) == 120);
static_assert(minutes(1) == 3600);

TEST_CASE("every is due on its offset, then every interval") {
  constexpr Every beat{10, 5};

  CHECK_FALSE(beat.due(0));
  CHECK(beat.due(5));
  CHECK_FALSE(beat.due(10));
  CHECK(beat.due(15));
  CHECK_FALSE(Every{}.due(0));
}

TEST_CASE("a cooldown is ready until started, then after its length") {
  Cooldown cooldown;
  CHECK(cooldown.ready(0));

  cooldown.start(10, 30);

  CHECK_FALSE(cooldown.ready(39));
  CHECK(cooldown.ready(40));
}

TEST_CASE("a phase knows its stage and how long it has been in it") {
  enum class Stage : uint8_t { CALM, SIEGE };
  Phase<Stage> phase;
  CHECK(phase.is(Stage::CALM));

  phase.enter(Stage::SIEGE, 100);

  CHECK(phase.is(Stage::SIEGE));
  CHECK(phase.age(160) == 60);
}
