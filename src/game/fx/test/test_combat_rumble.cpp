#include <array>
#include <catch2/catch_test_macros.hpp>
#include <game/fx/combat-rumble.h>

using namespace eng;
using namespace eng::game;

namespace {

constexpr Vec3 PLAYER{5.0F, 5.0F, 0.0F};

/// A cue of @p kind at @p at, fired by @p side.
CombatCue cueAt(CombatCueKind kind, Vec3 at, Faction side) {
  CombatCue cue;
  cue.kind = kind;
  cue.at = at;
  cue.side = side;
  cue.radius = kind == CombatCueKind::BLAST ? 1.5F : 0.0F;
  return cue;
}

}  // namespace

TEST_CASE("a player's own shot kicks through the trigger") {
  const auto kick = combatCueRumble(
      cueAt(CombatCueKind::SHOT_FIRED, {5.5F, 5.0F, 0.5F}, Faction::FRIENDLY),
      PLAYER);
  REQUIRE(input::isRumbling(kick));
  REQUIRE(kick.right_trigger > 0.0F);
  REQUIRE(kick.low == 0.0F);
}

TEST_CASE("someone else's shot is not felt") {
  const auto far = combatCueRumble(
      cueAt(CombatCueKind::SHOT_FIRED, {9.0F, 5.0F, 0.5F}, Faction::FRIENDLY),
      PLAYER);
  const auto hostile = combatCueRumble(
      cueAt(CombatCueKind::SHOT_FIRED, {5.5F, 5.0F, 0.5F}, Faction::HOSTILE),
      PLAYER);
  REQUIRE_FALSE(input::isRumbling(far));
  REQUIRE_FALSE(input::isRumbling(hostile));
}

TEST_CASE("a blast thumps harder the closer it goes off") {
  const auto close = combatCueRumble(
      cueAt(CombatCueKind::BLAST, {5.5F, 5.0F, 0.0F}, Faction::HOSTILE),
      PLAYER);
  const auto farther = combatCueRumble(
      cueAt(CombatCueKind::BLAST, {8.0F, 5.0F, 0.0F}, Faction::HOSTILE),
      PLAYER);
  const auto gone = combatCueRumble(
      cueAt(CombatCueKind::BLAST, {15.0F, 5.0F, 0.0F}, Faction::HOSTILE),
      PLAYER);
  REQUIRE(close.low > farther.low);
  REQUIRE(input::isRumbling(farther));
  REQUIRE_FALSE(input::isRumbling(gone));
}

TEST_CASE("a tick's cues feel like the strongest of them") {
  const std::array cues{
      cueAt(CombatCueKind::SHOT_FIRED, {5.5F, 5.0F, 0.5F}, Faction::FRIENDLY),
      cueAt(CombatCueKind::BLAST, {5.0F, 5.0F, 0.0F}, Faction::HOSTILE),
      cueAt(CombatCueKind::SHOT_HIT_WALL, {5.0F, 5.0F, 0.5F},
            Faction::FRIENDLY)};
  const auto felt = combatCuesRumble(cues, PLAYER);
  REQUIRE(felt.low == combatCueRumble(cues[1], PLAYER).low);
  REQUIRE(felt.right_trigger == combatCueRumble(cues[0], PLAYER).right_trigger);
}

TEST_CASE("a hit taken jolts harder the more of the bar it took") {
  REQUIRE_FALSE(input::isRumbling(hurtRumble(0, 5)));
  REQUIRE(hurtRumble(3, 5).low > hurtRumble(1, 5).low);
  REQUIRE(hurtRumble(9, 5).low == 1.0F);
}
