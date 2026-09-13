#include <algorithm>
#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <game/fx/combat-fx-preset.h>
#include <game/fx/combat-fx.h>
#include <set>
#include <string_view>

using Catch::Approx;
using namespace eng;
using namespace eng::game;

namespace {

/// Every kind of cue.
constexpr std::array<CombatCueKind, COMBAT_CUE_KIND_COUNT> KINDS{
    CombatCueKind::SHOT_FIRED, CombatCueKind::SHOT_HIT_BODY,
    CombatCueKind::SHOT_HIT_WALL, CombatCueKind::BLAST};

/// Below this saturation a colour reads as a tinted white or grey rather
/// than as a hue at all.
constexpr float GREYISH = 0.5F;

/// The hue of @p c in degrees, and its saturation, from its light alone.
std::pair<float, float> hueOf(const FxColor& c) {
  const float hi = std::max({c.r, c.g, c.b});
  const float lo = std::min({c.r, c.g, c.b});
  if (hi <= 0.0F || hi == lo) {
    return {0.0F, 0.0F};
  }
  const float span = hi - lo;
  float hue = 0.0F;
  if (hi == c.r) {
    hue = 60.0F * (c.g - c.b) / span;
  } else if (hi == c.g) {
    hue = 60.0F * ((c.b - c.r) / span + 2.0F);
  } else {
    hue = 60.0F * ((c.r - c.g) / span + 4.0F);
  }
  return {hue < 0.0F ? hue + 360.0F : hue, span / hi};
}

/// Whether @p c is a saturated colour inside hostile shots' hue band.
bool inHostileBand(const FxColor& c) {
  const auto [hue, saturation] = hueOf(c);
  return saturation >= GREYISH && hue >= HOSTILE_SHOT_HUE_MIN_DEGREES &&
         hue <= HOSTILE_SHOT_HUE_MAX_DEGREES;
}

/// A cue of @p kind at (1, 2, 0.9), heading +X.
CombatCue cueOf(CombatCueKind kind) {
  return {kind, {1.0F, 2.0F, 0.9F}, {0.5F, 0.0F}, 3.0F, Faction::HOSTILE};
}

}  // namespace

TEST_CASE("every cue kind plays particles and a flash of light") {
  for (const CombatCueKind kind : KINDS) {
    const FxEffect& effect = combatCueEffect(kind);
    REQUIRE_FALSE(effect.bursts.empty());
    REQUIRE(effect.flash.intensity > 0.0F);
    REQUIRE(effect.flash.range > 0.0F);
    REQUIRE(effect.flash.life > 0.0F);
  }
}

TEST_CASE("no effect uses the hue hostile projectiles are drawn in") {
  // Engine REQUIREMENTS §5.5. Checked at birth, halfway and at death, since
  // a particle fades between its two colours.
  for (const CombatCueKind kind : KINDS) {
    for (const FxBurst& burst : combatCueEffect(kind).bursts) {
      const FxColor& from = burst.look.color_start;
      const FxColor& to = burst.look.color_end;
      REQUIRE_FALSE(inHostileBand(from));
      REQUIRE_FALSE(inHostileBand(mixFxColor(from, to, 0.5F)));
      REQUIRE_FALSE(inHostileBand(to));
    }
  }
}

TEST_CASE("a muzzle flash points along the shot, from where it left") {
  const FxEmit emit = combatCueEmit(cueOf(CombatCueKind::SHOT_FIRED));
  REQUIRE(emit.at.x == 1.0F);
  REQUIRE(emit.at.z == 0.9F);
  REQUIRE(emit.direction.x == Approx(1.0F));
  REQUIRE(emit.direction.z == 0.0F);
  REQUIRE(emit.scale == 1.0F);
}

TEST_CASE("a wall throws its sparks back and up; a hit carries on through") {
  const FxEmit wall = combatCueEmit(cueOf(CombatCueKind::SHOT_HIT_WALL));
  REQUIRE(wall.direction.x < 0.0F);
  REQUIRE(wall.direction.z > 0.0F);
  const FxEmit body = combatCueEmit(cueOf(CombatCueKind::SHOT_HIT_BODY));
  REQUIRE(body.direction.x > 0.0F);
  REQUIRE(body.direction.z > 0.0F);
}

TEST_CASE("a blast plays upward, scaled to its radius") {
  const FxEmit emit = combatCueEmit(cueOf(CombatCueKind::BLAST));
  REQUIRE(emit.direction.z == 1.0F);
  REQUIRE(emit.scale == Approx(3.0F / COMBAT_FX_BLAST_RADIUS));
}

TEST_CASE("playing cues fills the effects world with what they play") {
  FxWorld world(1);
  const std::array<CombatCue, 2> cues{cueOf(CombatCueKind::SHOT_FIRED),
                                      cueOf(CombatCueKind::BLAST)};
  playCombatCues(world, cues);

  uint32_t particles = 0;
  for (const CombatCue& cue : cues) {
    for (const FxBurst& burst : combatCueEffect(cue.kind).bursts) {
      particles += burst.count;
    }
  }
  REQUIRE(world.particles.live == particles);
  REQUIRE(world.lights.live == 2);
}

TEST_CASE("every burst of every combat effect is a preset, named once") {
  size_t bursts = 0;
  for (const CombatCueKind kind : KINDS) {
    bursts += combatCueEffect(kind).bursts.size();
  }
  REQUIRE(combatFxPresets().size() == bursts);
  std::set<std::string_view> ids;
  for (const CombatFxPreset& preset : combatFxPresets()) {
    REQUIRE_FALSE(preset.name.empty());
    REQUIRE(ids.insert(preset.id).second);
    REQUIRE(findCombatFxPreset(preset.id) == &preset);
  }
  REQUIRE(findCombatFxPreset("no_such_effect") == nullptr);
}

TEST_CASE("a preset is the burst its effect throws, and its light") {
  const CombatFxPreset* sparks = findCombatFxPreset("wall_sparks");
  REQUIRE(sparks != nullptr);
  const FxEffect& wall = combatCueEffect(CombatCueKind::SHOT_HIT_WALL);
  REQUIRE(sparks->burst.count == wall.bursts[0].count);
  REQUIRE(sparks->flash.intensity == wall.flash.intensity);
  const CombatFxPreset* smoke = findCombatFxPreset("smoke");
  REQUIRE(smoke != nullptr);
  REQUIRE(smoke->flash.intensity == 0.0F);
}
