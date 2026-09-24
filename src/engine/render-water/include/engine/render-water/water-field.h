#pragma once

/// @file water-field.h
/// @brief The ripples on painted water: a height field stepped by the wave
/// equation, and how it is shaped, pushed and aged.
/// @par Threading
/// Main-thread-only (owns heap storage).

#include <cstdint>
#include <engine/core/pcg32.h>
#include <engine/math/vec2.h>
#include <engine/render-ground/ground-grid.h>
#include <engine/render-water/water-depth.h>
#include <engine/render-water/water-layer.h>
#include <vector>

namespace eng {

/// The pull that makes water level itself, in tiles a second squared. A
/// ripple on water `d` tiles deep travels at `sqrt(WATER_GRAVITY × d)` tiles
/// a second — the shallow-water wave speed — so it crawls across a puddle
/// and runs across a lake; a pond of the default depth carries it at 2.5.
inline constexpr float WATER_GRAVITY = 6.25f;

/// How hard the bottom drags a ripple, per second, in water a tile deep; it
/// grows as the water shallows, so a puddle stills in a moment where a lake
/// rolls on.
inline constexpr float WATER_BOTTOM_FRICTION = 0.25f;

/// The shallowest depth, in tiles, the bottom's drag is worked out at, so
/// the very edge of a bank damps hard rather than infinitely.
inline constexpr float WATER_FRICTION_MIN_DEPTH = 0.05f;

/// How quickly a ripple dies away on open water: the fraction of its motion
/// kept after a second is `exp(-WATER_DAMPING)`.
inline constexpr float WATER_DAMPING = 0.7f;

/// How much faster than on open water a ripple dies on the shore itself,
/// thinning to open water's rate `WATER_BEACH_TILES` out, so a wave runs up
/// a beach and is spent rather than bouncing off it.
inline constexpr float WATER_BEACH_DAMPING = 9.0f;

/// How far out from the shore, in tiles, a beach absorbs ripples.
inline constexpr float WATER_BEACH_TILES = 0.6f;

/// The fixed step the field is advanced by, in seconds. Frames of any
/// length are cut into these, so a ripple moves the same at any frame rate.
inline constexpr float WATER_STEP_SECONDS = 1.0f / 60.0f;

/// The most steps one frame runs. A frame longer than that — a stall, a
/// breakpoint — loses the rest rather than spending the next frame catching
/// up.
inline constexpr uint32_t WATER_MAX_STEPS_PER_FRAME = 4;

/// The most samples a field holds: Engine §7's budget for it. Water painted
/// over more than this at the fidelity's resolution is simulated at half
/// the resolution, and again, until it fits; water too wide to fit even at
/// one sample a tile is left flat.
inline constexpr uint32_t WATER_MAX_SAMPLES = 512U * 512U;

/// How far from the shore, in tiles, the water is deep: the shore distance
/// every sample carries stops counting there.
inline constexpr float WATER_SHORE_TILES = WATER_BANK_MAX_TILES;

/// Drops of drizzle that land a second on each tile of water, so open water
/// is never glass even when nothing is moving through it.
inline constexpr float WATER_DRIZZLE_PER_TILE = 0.12f;

/// The random stream the drizzle is drawn from (Engine §4.3). Presentation
/// only: nothing drawn from it reaches a tick.
inline constexpr uint64_t WATER_RNG_STREAM = 0x7761;

/// A rectangle of samples over a level's water, each with a height and the
/// speed it is moving at.
///
/// Structure of arrays, row by row from the south-west, each array
/// `width × height` long. Samples over land are *dry*: their height stays
/// zero, which is the shore every ripple meets. The rectangle covers only
/// the water's own bounds; everything outside it is dry too.
///
/// Presentation only, like every effect (ADR-002): it is stepped on the
/// frame's clock, uses `exp`, and its drizzle comes from a stream that is
/// never hashed. Two runs of it need not agree, and nothing reads it back.
/// @thread_safety Main-thread-only.
struct WaterField {
  /// The cell the south-west corner of the first sample lies in.
  GroundCell origin{};
  /// Samples along each row.
  uint32_t width = 0;
  /// Rows of samples.
  uint32_t height = 0;
  /// Samples along each side of a tile.
  uint32_t samples_per_tile = 0;
  /// How far each sample is above still water, in tiles.
  std::vector<float> level;
  /// How far each sample moves in one step, in tiles.
  std::vector<float> velocity;
  /// What each sample's velocity keeps through one step: less on a beach
  /// and in shallow water. Zero on a dry sample, which is what holds it
  /// still.
  std::vector<float> keep;
  /// How deep the water is at each sample, in tiles: its cells' depths
  /// blended between corners and shelved towards the bank. Zero when dry.
  std::vector<float> depth;
  /// How strongly each sample is pulled towards its neighbours in one step:
  /// `(wave speed × step ÷ spacing)²` at its depth.
  std::vector<float> pull;
  /// How far each sample is from the nearest dry one, in tiles, up to
  /// `WATER_SHORE_TILES`; zero when it is dry itself.
  std::vector<float> shore;
  /// How many samples are wet.
  uint32_t wet_count = 0;
  /// Seconds handed to `stepWaterField` that no step has taken yet.
  float pending_seconds = 0.0f;
  /// Drops of drizzle owed and not yet dropped.
  float drizzle_due = 0.0f;
  /// Where the drizzle lands.
  Pcg32 rng{0, WATER_RNG_STREAM};
};

/// Shape @p field over every cell of @p layer's water, as deep as it says,
/// at @p samples_per_tile — or fewer, when that would pass
/// `WATER_MAX_SAMPLES`. Every sample starts still. Zero samples a tile, or
/// no water, leaves the field empty.
void resetWaterField(WaterField& field, const WaterLayer& layer,
                     uint32_t samples_per_tile);

/// Advance @p field by @p seconds of the frame's clock, in fixed steps of
/// `WATER_STEP_SECONDS`, with drizzle falling as it goes.
void stepWaterField(WaterField& field, float seconds);

/// Push the water down in a bowl @p radius tiles across about @p at, by
/// @p depth tiles at its middle, which rings out as it springs back.
/// Returns whether any wet sample was touched: a push over land does
/// nothing.
bool disturbWaterField(WaterField& field, Vec2 at, float radius, float depth);

/// Whether the sample under @p at is wet.
[[nodiscard]] bool waterFieldWetAt(const WaterField& field, Vec2 at);

/// How much the water is moving: the sum over every sample of its height
/// and its speed, squared. Zero on still water.
[[nodiscard]] double waterFieldEnergy(const WaterField& field);

/// Whether @p field has any samples at all.
[[nodiscard]] inline bool waterFieldEmpty(const WaterField& field) {
  return field.wet_count == 0;
}

}  // namespace eng
