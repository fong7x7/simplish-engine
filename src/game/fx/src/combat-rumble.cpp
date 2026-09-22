#include <algorithm>
#include <cmath>
#include <game/fx/combat-rumble.h>

namespace eng::game {

namespace {

  /// How near the muzzle must be to the player, in tiles, for a shot to
  /// be theirs: a shot leaves from just in front of whoever fired it.
  constexpr float OWN_SHOT_REACH = 1.0F;
  /// How many radii out a blast is still felt, fading to nothing there.
  constexpr float BLAST_FELT_RADII = 3.0F;

  /// A player's own shot: a short kick through the trigger.
  constexpr input::GamepadRumble SHOT_KICK{
      .high = 0.25F, .right_trigger = 0.35F, .seconds = 0.06F};
  /// A blast at the player's feet; further off, the same scaled down.
  constexpr input::GamepadRumble BLAST_THUMP{
      .low = 0.9F, .high = 0.4F, .seconds = 0.35F};

  /// How far apart @p a and @p b are across the ground, in tiles.
  float groundDistance(Vec3 a, Vec3 b) {
    return std::hypot(a.x - b.x, a.y - b.y);
  }

  /// A blast @p distance tiles away with radius @p radius, as felt.
  input::GamepadRumble blastRumble(float distance, float radius) {
    const float reach = std::max(radius, 0.5F) * BLAST_FELT_RADII;
    const float strength = std::clamp(1.0F - distance / reach, 0.0F, 1.0F);
    return {.low = BLAST_THUMP.low * strength,
            .high = BLAST_THUMP.high * strength,
            .seconds = strength > 0.0F ? BLAST_THUMP.seconds : 0.0F};
  }

}  // namespace

input::GamepadRumble combatCueRumble(const CombatCue& cue, Vec3 player) {
  const float distance = groundDistance(cue.at, player);
  switch (cue.kind) {
    case CombatCueKind::SHOT_FIRED:
      return cue.side == Faction::FRIENDLY && distance <= OWN_SHOT_REACH
                 ? SHOT_KICK
                 : input::GamepadRumble{};
    case CombatCueKind::BLAST:
      return blastRumble(distance, cue.radius);
    case CombatCueKind::SHOT_HIT_BODY:
    case CombatCueKind::SHOT_HIT_WALL:
      break;
  }
  return {};
}

input::GamepadRumble combatCuesRumble(std::span<const CombatCue> cues,
                                      Vec3 player) {
  input::GamepadRumble felt;
  for (const CombatCue& cue : cues) {
    felt = input::strongerRumble(felt, combatCueRumble(cue, player));
  }
  return felt;
}

input::GamepadRumble hurtRumble(uint16_t lost, uint16_t max_health) {
  if (lost == 0 || max_health == 0) {
    return {};
  }
  const float share =
      std::min(1.0F, static_cast<float>(lost) / static_cast<float>(max_health));
  return {.low = 0.5F + 0.5F * share, .high = 0.6F, .seconds = 0.2F};
}

}  // namespace eng::game
