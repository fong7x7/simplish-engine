#include <algorithm>
#include <array>
#include <game/fx/combat-fx-preset.h>
#include <game/fx/combat-fx.h>

namespace eng::game {

namespace {

  // Every colour below is premultiplied linear light (`FxColor`): alpha zero
  // for a glow that only adds light, and dark with a high alpha for what
  // hides the scene behind it. None is a saturated orange — that band is
  // hostile projectiles' (Engine §5.5) — so fire reads yellow-white here.

  /// A shot leaving the muzzle: a short, bright core thrown along the shot
  /// and a few quick sparks.
  constexpr std::array<FxBurst, 2> MUZZLE_BURSTS{{
      {.count = 4,
       .spread_degrees = 12.0F,
       .speed_min = 1.5F,
       .speed_max = 3.0F,
       .life_min = 0.05F,
       .life_max = 0.08F,
       .look = {.size_start = 0.2F,
                .size_end = 0.05F,
                .color_start = {1.0F, 0.95F, 0.75F, 0.0F},
                .color_end = {0.4F, 0.38F, 0.25F, 0.0F}}},
      {.count = 5,
       .spread_degrees = 20.0F,
       .speed_min = 4.0F,
       .speed_max = 7.0F,
       .life_min = 0.08F,
       .life_max = 0.14F,
       .look = {.size_start = 0.035F,
                .size_end = 0.02F,
                .color_start = {1.0F, 0.95F, 0.7F, 0.0F},
                .color_end = {0.5F, 0.45F, 0.2F, 0.0F},
                .drag = 2.0F,
                .stretch = 0.03F}},
  }};

  /// A shot stopped by a wall: sparks that bounce back and fall, and a
  /// puff of grit.
  constexpr std::array<FxBurst, 2> WALL_BURSTS{{
      {.count = 10,
       .spread_degrees = 70.0F,
       .speed_min = 2.5F,
       .speed_max = 6.0F,
       .life_min = 0.15F,
       .life_max = 0.35F,
       .look = {.size_start = 0.03F,
                .size_end = 0.02F,
                .color_start = {1.0F, 0.97F, 0.8F, 0.0F},
                .color_end = {0.35F, 0.3F, 0.1F, 0.0F},
                .gravity = 9.0F,
                .drag = 1.5F,
                .stretch = 0.04F}},
      {.count = 3,
       .spread_degrees = 60.0F,
       .speed_min = 0.3F,
       .speed_max = 0.8F,
       .life_min = 0.35F,
       .life_max = 0.6F,
       .look = {.size_start = 0.1F,
                .size_end = 0.28F,
                .color_start = {0.12F, 0.11F, 0.1F, 0.35F},
                .color_end = {0.0F, 0.0F, 0.0F, 0.0F},
                .gravity = -0.3F,
                .drag = 2.0F,
                .spin = 40.0F,
                .shape = FxParticleShape::PUFF,
                .lighting = FxParticleLighting::LIT}},
  }};

  /// A shot that hits someone: a dark spray carried on through them.
  constexpr std::array<FxBurst, 1> BODY_BURSTS{{
      {.count = 8,
       .spread_degrees = 35.0F,
       .speed_min = 1.5F,
       .speed_max = 3.5F,
       .life_min = 0.25F,
       .life_max = 0.45F,
       .look = {.size_start = 0.06F,
                .size_end = 0.03F,
                .color_start = {0.35F, 0.02F, 0.04F, 0.9F},
                .color_end = {0.2F, 0.0F, 0.02F, 0.6F},
                .gravity = 8.0F,
                .drag = 1.0F}},
  }};

  /// Something going off: a fireball, embers thrown up and falling, and
  /// smoke that rises and spreads after them.
  constexpr std::array<FxBurst, 3> BLAST_BURSTS{{
      {.count = 28,
       .spread_degrees = 180.0F,
       .speed_min = 1.0F,
       .speed_max = 3.0F,
       .life_min = 0.25F,
       .life_max = 0.5F,
       .look = {.size_start = 0.3F,
                .size_end = 0.6F,
                .color_start = {1.0F, 0.92F, 0.55F, 0.0F},
                .color_end = {0.0F, 0.0F, 0.0F, 0.0F},
                .gravity = -1.0F,
                .drag = 3.0F}},
      {.count = 14,
       .spread_degrees = 70.0F,
       .speed_min = 3.0F,
       .speed_max = 6.0F,
       .life_min = 0.4F,
       .life_max = 0.8F,
       .look = {.size_start = 0.04F,
                .size_end = 0.025F,
                .color_start = {1.0F, 0.95F, 0.6F, 0.0F},
                .color_end = {0.4F, 0.35F, 0.1F, 0.0F},
                .gravity = 6.0F,
                .drag = 0.8F,
                .stretch = 0.03F}},
      {.count = 10,
       .spread_degrees = 60.0F,
       .speed_min = 0.4F,
       .speed_max = 1.2F,
       .life_min = 0.9F,
       .life_max = 1.5F,
       .look = {.size_start = 0.35F,
                .size_end = 0.9F,
                .color_start = {0.05F, 0.05F, 0.05F, 0.55F},
                .color_end = {0.0F, 0.0F, 0.0F, 0.0F},
                .gravity = -0.6F,
                .drag = 1.5F,
                .spin = 25.0F,
                .shape = FxParticleShape::PUFF,
                .lighting = FxParticleLighting::LIT}},
  }};

  /// The cloud a blast leaves standing where it went off: a body of smoke
  /// the effects pass marches a ray through, so it fills a doorway and
  /// wraps what it meets rather than cutting against it.
  ///
  /// It outlasts the blast's puffs by some seconds, which is the point —
  /// the smoke a fight leaves behind is cover, and a flat billboard
  /// standing in a corridor gives none.
  constexpr std::array<FxVolume, 1> BLAST_VOLUMES{{
      {.color = {0.05F, 0.05F, 0.055F, 1.0F},
       .density = 1.6F,
       .radius = 1.1F,
       .height = 0.8F,
       .growth = 0.35F,
       .rise = 0.3F,
       .life = 4.0F},
  }};

  /// The flash a shot leaving the muzzle throws.
  constexpr FxFlash MUZZLE_FLASH{{1.0F, 0.88F, 0.65F}, 2.2F, 3.0F, 0.07F};

  /// The flare of a shot hitting someone.
  constexpr FxFlash BODY_FLASH{{1.0F, 0.6F, 0.55F}, 0.8F, 1.5F, 0.06F};

  /// The flare of a shot a wall stops.
  constexpr FxFlash WALL_FLASH{{1.0F, 0.95F, 0.8F}, 1.2F, 1.8F, 0.08F};

  /// The light of something going off.
  constexpr FxFlash BLAST_FLASH{{1.0F, 0.9F, 0.65F}, 4.0F, 4.0F, 0.3F};

  /// A steady column of smoke, for a fire, a vent or a smouldering wreck:
  /// a few slow puffs an emitter throws over and over, living long enough
  /// to overlap into a plume and widening as they rise. No combat effect
  /// throws it — it is here so an emitter can.
  constexpr FxBurst PLUME_BURST{
      .count = 3,
      .spread_degrees = 22.0F,
      .speed_min = 0.35F,
      .speed_max = 0.9F,
      .life_min = 2.5F,
      .life_max = 4.0F,
      .look = {.size_start = 0.3F,
               .size_end = 1.5F,
               .color_start = {0.06F, 0.06F, 0.07F, 0.5F},
               .color_end = {0.0F, 0.0F, 0.0F, 0.0F},
               .gravity = -0.5F,
               .drag = 0.9F,
               .spin = 12.0F,
               .shape = FxParticleShape::PUFF,
               .lighting = FxParticleLighting::LIT}};

  /// Every cue kind's effect, in `CombatCueKind` order.
  constexpr std::array<FxEffect, COMBAT_CUE_KIND_COUNT> EFFECTS{{
      {MUZZLE_BURSTS, {}, MUZZLE_FLASH},
      {BODY_BURSTS, {}, BODY_FLASH},
      {WALL_BURSTS, {}, WALL_FLASH},
      {BLAST_BURSTS, BLAST_VOLUMES, BLAST_FLASH},
  }};

  /// Every burst above as a preset of its own. A burst that rides along
  /// with another's light — the muzzle's sparks, the grit, the embers and
  /// the smoke — throws none of its own, as in the effect it came from.
  constexpr std::array<CombatFxPreset, 9> PRESETS{{
      {"muzzle_flash", "Muzzle Flash", MUZZLE_BURSTS[0], MUZZLE_FLASH},
      {"muzzle_sparks", "Muzzle Sparks", MUZZLE_BURSTS[1], {}},
      {"wall_sparks", "Wall Sparks", WALL_BURSTS[0], WALL_FLASH},
      {"grit", "Grit", WALL_BURSTS[1], {}},
      {"hit_spray", "Hit Spray", BODY_BURSTS[0], BODY_FLASH},
      {"fireball", "Fireball", BLAST_BURSTS[0], BLAST_FLASH},
      {"embers", "Embers", BLAST_BURSTS[1], {}},
      {"smoke", "Smoke", BLAST_BURSTS[2], {}},
      {"smoke_plume", "Smoke Plume", PLUME_BURST, {}},
  }};

  static_assert(static_cast<size_t>(CombatCueKind::BLAST) + 1 ==
                    COMBAT_CUE_KIND_COUNT,
                "every cue kind has an effect, in order");

  /// How far up, for each unit along the ground, a wall's sparks are thrown.
  constexpr float WALL_SPARK_LIFT = 0.6F;

  /// How far up, for each unit along the shot, a hit's spray is thrown.
  constexpr float BODY_SPRAY_LIFT = 0.3F;

  /// @p heading as a unit direction along the floor, raised by @p lift
  /// for each unit of it.
  Vec3 along(Vec2 heading, float lift) {
    const Vec3 flat = Vec3::normalize({heading.x, heading.y, 0.0F});
    return {flat.x, flat.y, lift};
  }

}  // namespace

const FxEffect& combatCueEffect(CombatCueKind kind) {
  return EFFECTS[static_cast<size_t>(kind)];
}

FxEmit combatCueEmit(const CombatCue& cue) {
  switch (cue.kind) {
    case CombatCueKind::SHOT_FIRED:
      return {cue.at, along(cue.heading, 0.0F), 1.0F};
    case CombatCueKind::SHOT_HIT_BODY:
      return {cue.at, along(cue.heading, BODY_SPRAY_LIFT), 1.0F};
    case CombatCueKind::SHOT_HIT_WALL:
      return {cue.at, along(cue.heading * -1.0F, WALL_SPARK_LIFT), 1.0F};
    case CombatCueKind::BLAST:
      return {cue.at, {0.0F, 0.0F, 1.0F}, cue.radius / COMBAT_FX_BLAST_RADIUS};
  }
  return {cue.at, {}, 1.0F};
}

std::span<const CombatFxPreset> combatFxPresets() {
  return PRESETS;
}

const CombatFxPreset* findCombatFxPreset(std::string_view id) {
  for (const CombatFxPreset& preset : PRESETS) {
    if (preset.id == id) {
      return &preset;
    }
  }
  return nullptr;
}

void playCombatCues(FxWorld& world, std::span<const CombatCue> cues) {
  for (const CombatCue& cue : cues) {
    playFxEffect(world, combatCueEffect(cue.kind), combatCueEmit(cue));
  }
}

}  // namespace eng::game
