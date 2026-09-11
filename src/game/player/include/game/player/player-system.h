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

/// Ticks a player cannot be hurt again after a hit: three quarters of a
/// second.
inline constexpr uint64_t PLAYER_HURT_GRACE_TICKS = 45;

/// Ticks a teammate must stand by a downed player to revive them: two
/// seconds.
inline constexpr uint32_t PLAYER_REVIVE_TICKS = 120;

/// How near a downed player a teammate must stand to revive them, centre
/// to centre, in tiles.
inline constexpr float PLAYER_REVIVE_REACH_TILES = 1.5F;

/// How long a player can stay down before they are out of the run: thirty
/// seconds (Game REQUIREMENTS §3.3's revive window).
inline constexpr uint64_t PLAYER_DOWNED_WINDOW_TICKS = 1800;

/// Health segments a revived player gets back.
inline constexpr uint16_t PLAYER_REVIVED_HEALTH = 1;

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
/// other. A player who is not up does not move or turn.
void movePlayers(PlayerPool& pool, const sim::TickInput& input,
                 std::span<const physics::CollisionBox> obstacles);

/// Whether the player at dense index @p index is up: neither down nor out.
[[nodiscard]] bool playerIsUp(const PlayerPool& pool, uint32_t index);

/// The player at dense index @p index loses @p amount segments on @p tick,
/// unless they are not up or are in their grace after a hit. Down at none
/// left.
void hurtPlayer(PlayerPool& pool, uint32_t index, uint16_t amount,
                uint64_t tick);

/// §4.1 step 6's end: revive every downed player a teammate who is up has
/// stood by long enough, and put out of the run every one down past the
/// window — or down with no teammate up to revive them (Game §3.3: solo
/// death ends the run).
void updateDownedPlayers(PlayerPool& pool, uint64_t tick);

/// §4.1 step 8: destroy the players marked for it.
void compactPlayers(PlayerPool& pool);

/// Fold every player's state into a tick hash section.
void hashPlayers(const PlayerPool& pool, sim::StateHasher& hasher);

}  // namespace eng::game
