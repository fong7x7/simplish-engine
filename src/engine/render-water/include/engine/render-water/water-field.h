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
#include <engine/render-water/water-obstacle.h>
#include <span>
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

/// The most samples a field of flowing water holds: every step carries its
/// level, speed and foam downstream too, about as much again as the rest
/// of the step, so it has half the budget and a wide river is simulated a
/// step coarser than a lake as wide.
inline constexpr uint32_t WATER_MAX_FLOWING_SAMPLES = WATER_MAX_SAMPLES / 2;

/// How far from the shore, in tiles, the water is deep: the shore distance
/// every sample carries stops counting there.
inline constexpr float WATER_SHORE_TILES = WATER_BANK_MAX_TILES;

/// How far from the water, in tiles, the ground it has wet is darkened:
/// the wet band every dry sample carries its distance to the water for.
inline constexpr float WATER_WET_TILES = 0.35f;

/// How far from a bank, in tiles, flowing water is slowed by it: the flow
/// grows from nothing at the bank to its full speed this far out.
inline constexpr float WATER_FLOW_BANK_TILES = 0.5f;

/// How much slower the thickest fluid carries a ripple than water: its
/// wave speed is `1 − WATER_VISCOUS_SLOWING` of water's.
inline constexpr float WATER_VISCOUS_SLOWING = 0.6f;

/// How much faster the thickest fluid stills than water, added to
/// `WATER_DAMPING`.
inline constexpr float WATER_VISCOUS_DAMPING = 4.0f;

/// How much of the difference from its neighbours' motion a sample of the
/// thickest fluid gives up each step at `WATER_HIGH_SAMPLES_PER_TILE`:
/// viscosity spreading motion out, which stills a short ripple far sooner
/// than a long swell, as a thick fluid's surface does. Under the scheme's
/// limit of a quarter.
inline constexpr float WATER_VISCOUS_SPREAD = 0.2f;

/// How long foam lingers: what is left of it after this many seconds is
/// `1/e` of what there was.
inline constexpr float WATER_FOAM_SECONDS = 1.4f;

/// How much foam a push throws for each tile it sinks the water, past
/// `WATER_FOAM_CALM_DEPTH`, and a crest for each tile it rises past
/// `WATER_FOAM_CREST`: a wake leaves a trail of it and a blast a sheet.
inline constexpr float WATER_FOAM_PER_DEPTH = 12.0f;

/// The deepest push that throws no foam: a drop of drizzle's.
inline constexpr float WATER_FOAM_CALM_DEPTH = 0.015f;

/// How high a crest rises, in tiles, before it breaks into foam.
inline constexpr float WATER_FOAM_CREST = 0.04f;

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
  /// How far each dry sample is from the nearest wet one, in tiles, up to
  /// `WATER_WET_TILES`; zero when it is wet itself.
  std::vector<float> land;
  /// How much foam is on each sample, from 0 for none to 1: thrown by
  /// pushes and breaking crests, and thinning over `WATER_FOAM_SECONDS`.
  std::vector<float> foam;
  /// Which way and how fast each sample flows, in tiles a second, along x
  /// and along y: its cells' flow blended between corners and slowed
  /// towards the bank. Zero on a dry sample.
  std::vector<float> flow_x;
  /// Along y, as `flow_x`.
  std::vector<float> flow_y;
  /// Whether any sample flows, which is when a step carries the ripples
  /// and the foam downstream.
  bool flowing = false;
  /// Every sample that flows, by where it is in the arrays.
  std::vector<uint32_t> carried;
  /// For each of `carried`, the sample south-west of where its water was a
  /// step ago — worked out once, since the flow does not change until the
  /// field is shaped anew.
  std::vector<uint32_t> carried_from;
  /// For each of `carried`, how far east of that sample, and how far north,
  /// its water was, 0 to 1.
  std::vector<float> carried_x;
  /// North, as `carried_x`.
  std::vector<float> carried_y;
  /// How thick each sample's water is, 0 for water to 1 for the
  /// thickest: its cells' blended between corners.
  std::vector<float> viscosity;
  /// Whether any sample is thicker than water, which is when a step
  /// spreads its motion out.
  bool viscous = false;
  /// Room for the carried level, speed and foam of every flowing sample,
  /// three to a sample, kept so a step does not allocate.
  std::vector<float> scratch;
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
/// `WATER_MAX_SAMPLES` — dry under each of @p obstacles, which the water
/// goes round as it goes round a shore. Every sample starts still. Zero
/// samples a tile, or no water, leaves the field empty.
void resetWaterField(WaterField& field, const WaterLayer& layer,
                     uint32_t samples_per_tile,
                     std::span<const WaterObstacle> obstacles = {});

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
