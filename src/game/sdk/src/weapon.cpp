#include <engine/math/sin-cos.h>
#include <game/sdk/player-input.h>
#include <game/sdk/weapon.h>

namespace eng::game::sdk {

namespace {

  /// How far from a player's feet a shot leaves: past their body, so it
  /// never starts inside them. A player's radius and a projectile's.
  constexpr float MUZZLE_TILES = 0.3F + 0.12F;

  /// @p aim turned @p degrees counterclockwise.
  Vec2 turned(Vec2 aim, float degrees) {
    const math::SinCos turn = math::sinCosDegrees(degrees);
    return {aim.x * turn.cos - aim.y * turn.sin,
            aim.x * turn.sin + aim.y * turn.cos};
  }

  /// The @p k-th of @p weapon's pellets, turned from the aim.
  float pelletDegrees(const Weapon& weapon, uint32_t k) {
    if (weapon.pellets < 2) {
      return 0.0F;
    }
    const float step =
        weapon.spread_degrees / static_cast<float>(weapon.pellets - 1);
    return -weapon.spread_degrees * 0.5F + step * static_cast<float>(k);
  }

}  // namespace

bool fireWeapon(GameLogicWorld& world, const LogicPlayer& player,
                const Weapon& weapon, Cooldown& cooldown) {
  if (!firing(world, player) || !cooldown.ready(world.tick())) {
    return false;
  }
  const Vec3 muzzle{player.position.x + player.aim.x * MUZZLE_TILES,
                    player.position.y + player.aim.y * MUZZLE_TILES,
                    player.position.z};
  for (uint32_t k = 0; k < weapon.pellets; ++k) {
    world.fireShot({.from = muzzle,
                    .direction = turned(player.aim, pelletDegrees(weapon, k)),
                    .speed = weapon.speed,
                    .damage = weapon.damage});
  }
  cooldown.start(world.tick(), weapon.refire_ticks);
  return true;
}

}  // namespace eng::game::sdk
