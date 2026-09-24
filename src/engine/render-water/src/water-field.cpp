#include <algorithm>
#include <cmath>
#include <engine/render-water/water-corners.h>
#include <engine/render-water/water-field.h>
#include <limits>
#include <numbers>
#include <utility>

namespace eng {

namespace {

  constexpr float PI = std::numbers::pi_v<float>;

  /// The largest `(speed × step ÷ spacing)²` a step is taken with. The
  /// scheme is stable up to a half; this keeps clear of the edge whatever
  /// resolution the budget leaves a field at.
  constexpr float MAX_COURANT_SQUARED = 0.45f;

  /// How deep one drop of drizzle pushes, in tiles, and how wide it is.
  constexpr float DRIZZLE_DEPTH = 0.01f;
  constexpr float DRIZZLE_RADIUS = 0.2f;

  /// How many samples a drop tries before giving up on landing: the field
  /// is mostly water, so this almost never runs out.
  constexpr uint32_t DRIZZLE_TRIES = 8;

  /// The chamfer distance between diagonal neighbours, in samples.
  constexpr float DIAGONAL = std::numbers::sqrt2_v<float>;

  /// Where sample (@p x, @p y) lives in the arrays.
  size_t slotOf(const WaterField& field, uint32_t x, uint32_t y) {
    return static_cast<size_t>(y) * field.width + x;
  }

  /// The resolution @p tiles can be simulated at within the budget,
  /// starting from @p samples_per_tile and halving; 0 when not even one a
  /// tile fits.
  uint32_t fittingResolution(GroundRect tiles, uint32_t samples_per_tile) {
    const auto area = static_cast<uint64_t>(tiles.width) *
                      static_cast<uint64_t>(tiles.height);
    uint32_t resolution = samples_per_tile;
    while (resolution > 0 &&
           area * resolution * resolution > WATER_MAX_SAMPLES) {
      resolution /= 2;
    }
    return resolution;
  }

  /// Size every array for @p tiles at @p resolution, all still and dry.
  void sizeField(WaterField& field, GroundRect tiles, uint32_t resolution) {
    field.origin = {tiles.x, tiles.y};
    field.samples_per_tile = resolution;
    field.width = static_cast<uint32_t>(tiles.width) * resolution;
    field.height = static_cast<uint32_t>(tiles.height) * resolution;
    const size_t count = static_cast<size_t>(field.width) * field.height;
    field.level.assign(count, 0.0f);
    field.velocity.assign(count, 0.0f);
    field.keep.assign(count, 0.0f);
    field.depth.assign(count, 0.0f);
    field.pull.assign(count, 0.0f);
    field.shore.assign(count, 0.0f);
    field.wet_count = 0;
    field.pending_seconds = 0.0f;
    field.drizzle_due = 0.0f;
  }

  /// Mark wet every sample whose cell is painted @p water, as 1 in `keep`
  /// for now; `settleKeep` turns that into the damping.
  void markWet(WaterField& field, const WaterLayer& layer) {
    const uint32_t n = field.samples_per_tile;
    for (uint32_t y = 0; y < field.height; ++y) {
      for (uint32_t x = 0; x < field.width; ++x) {
        const GroundCell cell{field.origin.x + static_cast<int32_t>(x / n),
                              field.origin.y + static_cast<int32_t>(y / n)};
        if (layer.depth.at(cell) != 0) {
          field.keep[slotOf(field, x, y)] = 1.0f;
          ++field.wet_count;
        }
      }
    }
  }

  /// @p distance at (@p x, @p y), or 0 — dry — past the field's edge.
  float distanceAt(const WaterField& field, const std::vector<float>& distance,
                   int64_t x, int64_t y) {
    if (x < 0 || y < 0 || std::cmp_greater_equal(x, field.width) ||
        std::cmp_greater_equal(y, field.height)) {
      return 0.0f;
    }
    return distance[slotOf(field, static_cast<uint32_t>(x),
                           static_cast<uint32_t>(y))];
  }

  /// One sample a sweep of the distance transform visits, and the way the
  /// sweep runs: +1 forward from the south-west, −1 back from the
  /// north-east.
  struct Visit {
    /// The sample's column.
    int64_t x = 0;
    /// The sample's row.
    int64_t y = 0;
    /// Which way the sweep runs.
    int64_t way = 1;
  };

  /// The chamfer pass over one sample: its distance, or one more than the
  /// nearest of the four neighbours the sweep has already visited.
  float chamfer(const WaterField& field, const std::vector<float>& d,
                Visit at) {
    const int64_t x = at.x;
    const int64_t y = at.y;
    const int64_t way = at.way;
    const float own =
        d[slotOf(field, static_cast<uint32_t>(x), static_cast<uint32_t>(y))];
    return std::min({own, distanceAt(field, d, x - way, y) + 1.0f,
                     distanceAt(field, d, x, y - way) + 1.0f,
                     distanceAt(field, d, x - way, y - way) + DIAGONAL,
                     distanceAt(field, d, x + way, y - way) + DIAGONAL});
  }

  /// One sweep of the distance transform, the way @p way runs.
  void sweep(const WaterField& field, std::vector<float>& d, int64_t way) {
    const auto w = static_cast<int64_t>(field.width);
    const auto h = static_cast<int64_t>(field.height);
    for (int64_t row = 0; row < h; ++row) {
      for (int64_t col = 0; col < w; ++col) {
        const int64_t x = way > 0 ? col : w - 1 - col;
        const int64_t y = way > 0 ? row : h - 1 - row;
        d[slotOf(field, static_cast<uint32_t>(x), static_cast<uint32_t>(y))] =
            chamfer(field, d, {x, y, way});
      }
    }
  }

  /// Every wet sample's distance to the nearest dry one — the edge of the
  /// field counting as dry — in tiles, capped at `WATER_SHORE_TILES`.
  void measureShore(WaterField& field) {
    std::vector<float> d(field.keep.size());
    for (size_t i = 0; i < d.size(); ++i) {
      d[i] = field.keep[i] > 0.0f ? std::numeric_limits<float>::max() : 0.0f;
    }
    sweep(field, d, 1);
    sweep(field, d, -1);
    const float tile = 1.0f / static_cast<float>(field.samples_per_tile);
    for (size_t i = 0; i < d.size(); ++i) {
      field.shore[i] = std::min(d[i] * tile, WATER_SHORE_TILES);
    }
  }

  /// Turn each wet sample's mark into what its velocity keeps through one
  /// step: open water's damping, and a beach's on top of it near the shore.
  void settleKeep(WaterField& field) {
    for (size_t i = 0; i < field.keep.size(); ++i) {
      if (field.keep[i] <= 0.0f) {
        continue;
      }
      const float beach =
          std::max(0.0f, 1.0f - field.shore[i] / WATER_BEACH_TILES);
      const float bottom = WATER_BOTTOM_FRICTION /
                           std::max(field.depth[i], WATER_FRICTION_MIN_DEPTH);
      const float damping =
          WATER_DAMPING + WATER_BEACH_DAMPING * beach + bottom;
      field.keep[i] = std::exp(-damping * WATER_STEP_SECONDS);
    }
  }

  /// @p field's level at (@p x, @p y); the caller keeps inside the edge.
  float levelAt(const WaterField& field, uint32_t x, uint32_t y) {
    return field.level[slotOf(field, x, y)];
  }

  /// Pull every wet sample towards the mean of its four neighbours. The
  /// field's edge row is always dry, since the field is only as big as the
  /// water, so the loop never reads past it.
  void accelerate(WaterField& field) {
    for (uint32_t y = 1; y + 1 < field.height; ++y) {
      for (uint32_t x = 1; x + 1 < field.width; ++x) {
        const size_t i = slotOf(field, x, y);
        const float laplacian =
            levelAt(field, x - 1, y) + levelAt(field, x + 1, y) +
            levelAt(field, x, y - 1) + levelAt(field, x, y + 1) -
            4.0f * field.level[i];
        field.velocity[i] =
            (field.velocity[i] + field.pull[i] * laplacian) * field.keep[i];
      }
    }
  }

  /// Where sample @p i's middle is, in tiles.
  Vec2 sampleCentre(const WaterField& field, size_t i) {
    const float tile = 1.0f / static_cast<float>(field.samples_per_tile);
    const size_t column = i % field.width;
    const size_t row = i / field.width;
    return {static_cast<float>(field.origin.x) +
                (static_cast<float>(column) + 0.5f) * tile,
            static_cast<float>(field.origin.y) +
                (static_cast<float>(row) + 0.5f) * tile};
  }

  /// How strongly water @p depth tiles deep pulls a sample towards its
  /// neighbours in one step, at @p samples_per_tile.
  float pullAt(float depth, uint32_t samples_per_tile) {
    const float speed = std::sqrt(WATER_GRAVITY * depth);
    const float courant =
        speed * WATER_STEP_SECONDS * static_cast<float>(samples_per_tile);
    return std::min(courant * courant, MAX_COURANT_SQUARED);
  }

  /// Every wet sample's depth — its cells' blended between corners and
  /// shelved towards the bank — and how hard that depth pulls it.
  void settleDepth(WaterField& field, const WaterLayer& layer) {
    const auto n = static_cast<int32_t>(field.samples_per_tile);
    const WaterCorners corners =
        makeWaterCorners(layer, {field.origin.x, field.origin.y,
                                 static_cast<int32_t>(field.width) / n,
                                 static_cast<int32_t>(field.height) / n});
    for (size_t i = 0; i < field.keep.size(); ++i) {
      if (field.keep[i] > 0.0f) {
        const float full = waterSampleAt(corners, sampleCentre(field, i)).depth;
        field.depth[i] = full * waterBankShelf(full, field.shore[i]);
        field.pull[i] = pullAt(field.depth[i], field.samples_per_tile);
      }
    }
  }

  /// Drop one drop of drizzle on a wet sample picked at random.
  void dropOne(WaterField& field) {
    const auto count = static_cast<uint32_t>(field.keep.size());
    for (uint32_t attempt = 0; attempt < DRIZZLE_TRIES; ++attempt) {
      const uint32_t i = field.rng.nextBelow(count);
      if (field.keep[i] > 0.0f) {
        disturbWaterField(field, sampleCentre(field, i), DRIZZLE_RADIUS,
                          DRIZZLE_DEPTH * (0.5f + field.rng.nextUnitFloat()));
        return;
      }
    }
  }

  /// Let one step's worth of drizzle fall.
  void drizzle(WaterField& field) {
    const auto per_tile = static_cast<float>(field.samples_per_tile);
    const float tiles =
        static_cast<float>(field.wet_count) / (per_tile * per_tile);
    field.drizzle_due += tiles * WATER_DRIZZLE_PER_TILE * WATER_STEP_SECONDS;
    while (field.drizzle_due >= 1.0f) {
      field.drizzle_due -= 1.0f;
      dropOne(field);
    }
  }

  /// Advance @p field by one fixed step.
  void stepOnce(WaterField& field) {
    accelerate(field);
    for (size_t i = 0; i < field.level.size(); ++i) {
      field.level[i] += field.velocity[i];
    }
    drizzle(field);
  }

  /// The bowl's depth at @p distance from its middle: all of it there,
  /// easing to none at @p radius.
  float bowl(float distance, float radius) {
    return 0.5f * (1.0f + std::cos(PI * distance / radius));
  }

  /// The sample index along one axis that @p tiles lies in, from an origin
  /// at @p origin, clamped into the field's @p count samples.
  uint32_t sampleIndex(float tiles, int32_t origin, uint32_t resolution,
                       uint32_t count) {
    const float at =
        (tiles - static_cast<float>(origin)) * static_cast<float>(resolution);
    const float last = static_cast<float>(count) - 1.0f;
    return static_cast<uint32_t>(std::clamp(std::floor(at), 0.0f, last));
  }

  /// The samples a square @p reach tiles either side of @p at covers,
  /// clamped to the field.
  struct SampleSpan {
    /// First column.
    uint32_t x0 = 0;
    /// Last column.
    uint32_t x1 = 0;
    /// First row.
    uint32_t y0 = 0;
    /// Last row.
    uint32_t y1 = 0;
  };

  /// The span of samples within @p reach of @p at on either axis.
  SampleSpan spanAround(const WaterField& field, Vec2 at, float reach) {
    const uint32_t n = field.samples_per_tile;
    return {sampleIndex(at.x - reach, field.origin.x, n, field.width),
            sampleIndex(at.x + reach, field.origin.x, n, field.width),
            sampleIndex(at.y - reach, field.origin.y, n, field.height),
            sampleIndex(at.y + reach, field.origin.y, n, field.height)};
  }

  /// How wide a push is and how deep at its middle, both in tiles.
  struct Push {
    /// How far from its middle it reaches.
    float reach = 0.0f;
    /// How far it pushes its middle down.
    float depth = 0.0f;
  };

  /// Push sample @p i down by as much of @p push as reaches it from @p at.
  /// Returns whether it was wet and within reach.
  bool pushSample(WaterField& field, size_t i, Vec2 at, Push push) {
    const float distance = Vec2::distance(sampleCentre(field, i), at);
    if (field.keep[i] <= 0.0f || distance >= push.reach) {
      return false;
    }
    field.level[i] -= push.depth * bowl(distance, push.reach);
    return true;
  }

}  // namespace

void resetWaterField(WaterField& field, const WaterLayer& layer,
                     uint32_t samples_per_tile) {
  GroundRect tiles = waterLayerBounds(layer);
  // A tile of dry land all round, so every wet sample has four neighbours
  // inside the field and the step never has to ask where the edge is.
  tiles = {tiles.x - 1, tiles.y - 1, tiles.width + 2, tiles.height + 2};
  const uint32_t resolution =
      tiles.width > 2 ? fittingResolution(tiles, samples_per_tile) : 0;
  sizeField(field, resolution > 0 ? tiles : GroundRect{}, resolution);
  if (resolution == 0) {
    return;
  }
  markWet(field, layer);
  measureShore(field);
  settleDepth(field, layer);
  settleKeep(field);
}

void stepWaterField(WaterField& field, float seconds) {
  if (waterFieldEmpty(field) || !(seconds > 0.0f)) {
    return;
  }
  field.pending_seconds = std::min(
      field.pending_seconds + seconds,
      WATER_STEP_SECONDS * static_cast<float>(WATER_MAX_STEPS_PER_FRAME));
  while (field.pending_seconds >= WATER_STEP_SECONDS) {
    stepOnce(field);
    field.pending_seconds -= WATER_STEP_SECONDS;
  }
}

bool disturbWaterField(WaterField& field, Vec2 at, float radius, float depth) {
  if (waterFieldEmpty(field)) {
    return false;
  }
  const float reach =
      std::max(radius, 1.0f / static_cast<float>(field.samples_per_tile));
  const SampleSpan span = spanAround(field, at, reach);
  bool touched = false;
  for (uint32_t y = span.y0; y <= span.y1; ++y) {
    for (uint32_t x = span.x0; x <= span.x1; ++x) {
      touched |= pushSample(field, slotOf(field, x, y), at, {reach, depth});
    }
  }
  return touched;
}

bool waterFieldWetAt(const WaterField& field, Vec2 at) {
  if (waterFieldEmpty(field)) {
    return false;
  }
  const auto n = static_cast<float>(field.samples_per_tile);
  const float fx = (at.x - static_cast<float>(field.origin.x)) * n;
  const float fy = (at.y - static_cast<float>(field.origin.y)) * n;
  // Asked this way round so a point that is not a number is outside.
  const bool inside = fx >= 0.0f && fy >= 0.0f &&
                      fx < static_cast<float>(field.width) &&
                      fy < static_cast<float>(field.height);
  if (!inside) {
    return false;
  }
  return field.keep[slotOf(field, static_cast<uint32_t>(fx),
                           static_cast<uint32_t>(fy))] > 0.0f;
}

double waterFieldEnergy(const WaterField& field) {
  double energy = 0.0;
  for (size_t i = 0; i < field.level.size(); ++i) {
    energy += static_cast<double>(field.level[i]) * field.level[i] +
              static_cast<double>(field.velocity[i]) * field.velocity[i];
  }
  return energy;
}

}  // namespace eng
