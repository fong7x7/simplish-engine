#pragma once

/// @file player-system.h
/// @brief What the simulation does to players each tick.
/// @par Threading
/// Main-thread-only; called from the tick's phases.

#include <cstdint>
#include <engine/math/vec3.h>
#include <engine/physics/collision-box.h>
#include <engine/sim/entity-handle.h>
#include <engine/sim/state-hasher.h>
#include <engine/sim/tick-input.h>
#include <game/content/character-definition.h>
#include <game/player/player-pool.h>
#include <optional>
#include <span>

namespace eng::game {

/// How wide a player is to collision: the radius of the circle they stand
/// in, in tiles. Narrower than a tile, so two can pass between props a
/// tile apart.
inline constexpr float PLAYER_RADIUS_TILES = 0.3F;

/// How tall a player is to collision, in tiles. Anything wholly above this
/// — a sign, a beam — is walked under.
inline constexpr float PLAYER_HEIGHT_TILES = 1.5F;

/// Add a player driven by input slot @p input_slot, feet at @p at, aiming
/// along world +X, playing as @p character — whose speed and health they
/// take. Nothing when the pool is full.
std::optional<sim::EntityHandle>
spawnPlayer(PlayerPool& pool, uint8_t input_slot, Vec3 at,
            const CharacterDefinition& character);

/// §4.1 step 2: move every player by its stick, keep them out of
/// @p obstacles, and take up their aim.
///
/// Movement is in world X and Y, at the player's own `move_speed` for a
/// full stick and proportionally less for a partial one. A player who walks
/// into an obstacle stops against it, and one who walks into it at an angle
/// slides along it. Every player is resolved every tick, moving or not, so
/// one who spawned inside an obstacle is out of it after the first tick.
/// Height is not touched: there is no ground to follow yet, so a player
/// stays at the height they spawned at. Players do not collide with each
/// other.
void movePlayers(PlayerPool& pool, const sim::TickInput& input,
                 std::span<const physics::CollisionBox> obstacles);

/// §4.1 step 8: destroy the players marked for it.
void compactPlayers(PlayerPool& pool);

/// Fold every player's state into a tick hash section.
void hashPlayers(const PlayerPool& pool, sim::StateHasher& hasher);

}  // namespace eng::game
