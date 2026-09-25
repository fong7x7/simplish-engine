#include <algorithm>
#include <cmath>
#include <engine/render-water/water-corners.h>
#include <engine/render-water/water-fidelity.h>
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

  /// The resolution @p tiles can be simulated at within @p budget samples,
  /// starting from @p samples_per_tile and halving; 0 when not even one a
  /// tile fits.
  uint32_t fittingResolution(GroundRect tiles, uint32_t samples_per_tile,
                             uint32_t budget) {
    const auto area = static_cast<uint64_t>(tiles.width) *
                      static_cast<uint64_t>(tiles.height);
    uint32_t resolution = samples_per_tile;
    while (resolution > 0 && area * resolution * resolution > budget) {
      resolution /= 2;
    }
    return resolution;
  }

  /// The samples @p layer's water may have: fewer when any of it flows.
  uint32_t budgetFor(const WaterLayer& layer) {
    const std::vector<uint8_t>& speeds = layer.flow_speed.cells();
    const bool flows =
        std::ranges::any_of(speeds, [](uint8_t speed) { return speed != 0; });
    return flows ? WATER_MAX_FLOWING_SAMPLES : WATER_MAX_SAMPLES;
  }

  /// Every array @p count samples long, all still and dry.
  void clearArrays(WaterField& field, size_t count) {
    field.level.assign(count, 0.0f);
    field.velocity.assign(count, 0.0f);
    field.keep.assign(count, 0.0f);
    field.depth.assign(count, 0.0f);
    field.pull.assign(count, 0.0f);
    field.shore.assign(count, 0.0f);
    field.land.assign(count, WATER_WET_TILES);
    field.foam.assign(count, 0.0f);
    field.flow_x.assign(count, 0.0f);
    field.flow_y.assign(count, 0.0f);
    field.viscosity.assign(count, 0.0f);
  }

  /// Size every array for @p tiles at @p resolution, all still and dry.
  void sizeField(WaterField& field, GroundRect tiles, uint32_t resolution) {
    field.origin = {tiles.x, tiles.y};
    field.samples_per_tile = resolution;
    field.width = static_cast<uint32_t>(tiles.width) * resolution;
    field.height = static_cast<uint32_t>(tiles.height) * resolution;
    clearArrays(field, static_cast<size_t>(field.width) * field.height);
    field.flowing = false;
    field.viscous = false;
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

  /// The first sample along one axis whose middle is at or past @p at,
  /// @p origin being the tile the axis's first sample starts in, held to
  /// [0, @p count].
  uint32_t sampleFrom(float at, int32_t origin, const WaterField& field,
                      uint32_t count) {
    const float n = static_cast<float>(field.samples_per_tile);
    const float first = std::ceil((at - static_cast<float>(origin)) * n - 0.5f);
    return static_cast<uint32_t>(
        std::clamp(first, 0.0f, static_cast<float>(count)));
  }

  /// Dry every wet sample whose middle lies under @p obstacle.
  void dryUnder(WaterField& field, const WaterObstacle& obstacle) {
    const uint32_t x0 =
        sampleFrom(obstacle.min.x, field.origin.x, field, field.width);
    const uint32_t x1 =
        sampleFrom(obstacle.max.x, field.origin.x, field, field.width);
    const uint32_t y0 =
        sampleFrom(obstacle.min.y, field.origin.y, field, field.height);
    const uint32_t y1 =
        sampleFrom(obstacle.max.y, field.origin.y, field, field.height);
    for (uint32_t y = y0; y < y1; ++y) {
      for (uint32_t x = x0; x < x1; ++x) {
        float& keep = field.keep[slotOf(field, x, y)];
        field.wet_count -= keep > 0.0f ? 1U : 0U;
        keep = 0.0f;
      }
    }
  }

  /// A distance transform over the field's samples, and what lies past
  /// its edge.
  struct Distances {
    /// The field whose samples these are.
    const WaterField& field;
    /// Each sample's distance so far, in samples.
    std::vector<float> d;
    /// The distance past the field's edge.
    float outside = 0.0f;
  };

  /// The distance at (@p x, @p y), or what lies past the field's edge.
  float distanceAt(const Distances& in, int64_t x, int64_t y) {
    if (x < 0 || y < 0 || std::cmp_greater_equal(x, in.field.width) ||
        std::cmp_greater_equal(y, in.field.height)) {
      return in.outside;
    }
    return in.d[slotOf(in.field, static_cast<uint32_t>(x),
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
  float chamfer(const Distances& in, Visit at) {
    const int64_t x = at.x;
    const int64_t y = at.y;
    const int64_t way = at.way;
    return std::min({distanceAt(in, x, y), distanceAt(in, x - way, y) + 1.0f,
                     distanceAt(in, x, y - way) + 1.0f,
                     distanceAt(in, x - way, y - way) + DIAGONAL,
                     distanceAt(in, x + way, y - way) + DIAGONAL});
  }

  /// One sweep of the distance transform, the way @p way runs.
  void sweep(Distances& in, int64_t way) {
    const auto w = static_cast<int64_t>(in.field.width);
    const auto h = static_cast<int64_t>(in.field.height);
    for (int64_t row = 0; row < h; ++row) {
      for (int64_t col = 0; col < w; ++col) {
        const int64_t x = way > 0 ? col : w - 1 - col;
        const int64_t y = way > 0 ? row : h - 1 - row;
        in.d[slotOf(in.field, static_cast<uint32_t>(x),
                    static_cast<uint32_t>(y))] = chamfer(in, {x, y, way});
      }
    }
  }

  /// Every sample's distance, in tiles, to the nearest one @p seed picks —
  /// with @p outside past the field's edge — capped at @p cap, into @p out.
  template <typename Seed>
  void measure(const WaterField& field, Seed seed, float outside,
               std::vector<float>& out) {
    constexpr float FAR = std::numeric_limits<float>::max();
    Distances in{field, std::vector<float>(field.keep.size()), outside};
    for (size_t i = 0; i < in.d.size(); ++i) {
      in.d[i] = seed(i) ? 0.0f : FAR;
    }
    sweep(in, 1);
    sweep(in, -1);
    const float tile = 1.0f / static_cast<float>(field.samples_per_tile);
    out.resize(in.d.size());
    for (size_t i = 0; i < in.d.size(); ++i) {
      out[i] = in.d[i] * tile;
    }
  }

  /// Every wet sample's distance to the nearest dry one — the edge of the
  /// field counting as dry — capped at `WATER_SHORE_TILES`; and every dry
  /// one's to the nearest wet one, capped at `WATER_WET_TILES`.
  void measureShore(WaterField& field) {
    const auto wet = [&](size_t i) {
      return field.keep[i] > 0.0f;
    };
    measure(field, [&](size_t i) { return !wet(i); }, 0.0f, field.shore);
    measure(field, wet, std::numeric_limits<float>::max(), field.land);
    // A dry sample's distance runs from the water's edge, half a sample
    // short of the wet one's middle.
    const float half = 0.5f / static_cast<float>(field.samples_per_tile);
    for (size_t i = 0; i < field.shore.size(); ++i) {
      field.shore[i] = std::min(field.shore[i], WATER_SHORE_TILES);
      field.land[i] = std::clamp(field.land[i] - half, 0.0f, WATER_WET_TILES);
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
      const float damping = WATER_DAMPING + WATER_BEACH_DAMPING * beach +
                            bottom + WATER_VISCOUS_DAMPING * field.viscosity[i];
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

  /// Sample @p i's depth and flow from @p water, the blend of its cells
  /// there, shelved and slowed towards the bank.
  void settleSample(WaterField& field, size_t i, const WaterSample& water) {
    field.depth[i] = water.depth * waterBankShelf(water.depth, field.shore[i]);
    const float slowed_by = 1.0f - WATER_VISCOUS_SLOWING * water.viscosity;
    field.pull[i] =
        pullAt(field.depth[i], field.samples_per_tile) * slowed_by * slowed_by;
    field.viscosity[i] = water.viscosity;
    field.viscous |= water.viscosity > 0.0f;
    const float slowed = std::min(1.0f, field.shore[i] / WATER_FLOW_BANK_TILES);
    field.flow_x[i] = water.flow.x * slowed;
    field.flow_y[i] = water.flow.y * slowed;
    field.flowing |= field.flow_x[i] != 0.0f || field.flow_y[i] != 0.0f;
  }

  /// Every wet sample's depth — its cells' blended between corners and
  /// shelved towards the bank — how hard that depth pulls it, and which
  /// way it flows.
  void settleDepth(WaterField& field, const WaterLayer& layer) {
    const auto n = static_cast<int32_t>(field.samples_per_tile);
    const WaterCorners corners =
        makeWaterCorners(layer, {field.origin.x, field.origin.y,
                                 static_cast<int32_t>(field.width) / n,
                                 static_cast<int32_t>(field.height) / n});
    for (size_t i = 0; i < field.keep.size(); ++i) {
      if (field.keep[i] > 0.0f) {
        settleSample(field, i, waterSampleAt(corners, sampleCentre(field, i)));
      }
    }
  }

  /// Where sample @p i's water was a step ago, for `carry` to read it from.
  void traceUpstream(WaterField& field, size_t i) {
    const float reach =
        WATER_STEP_SECONDS * static_cast<float>(field.samples_per_tile);
    const float x =
        static_cast<float>(i % field.width) - field.flow_x[i] * reach;
    const float y =
        static_cast<float>(i / field.width) - field.flow_y[i] * reach;
    const float x0 = std::floor(x);
    const float y0 = std::floor(y);
    field.carried.push_back(static_cast<uint32_t>(i));
    field.carried_from.push_back(static_cast<uint32_t>(y0) * field.width +
                                 static_cast<uint32_t>(x0));
    field.carried_x.push_back(x - x0);
    field.carried_y.push_back(y - y0);
  }

  /// Work out, once, where every flowing sample's water comes from. A wet
  /// sample is never on the field's edge and a step never carries water a
  /// whole sample, so the four samples round where it was are all inside.
  void traceFlow(WaterField& field) {
    field.carried.clear();
    field.carried_from.clear();
    field.carried_x.clear();
    field.carried_y.clear();
    for (size_t i = 0; i < field.keep.size(); ++i) {
      if (field.flow_x[i] != 0.0f || field.flow_y[i] != 0.0f) {
        traceUpstream(field, i);
      }
    }
    field.scratch.assign(3 * field.carried.size(), 0.0f);
  }

  /// @p a to @p b, @p t of the way: plain arithmetic, where `std::lerp`
  /// spends time on guarantees a step does not need.
  float mixed(float a, float b, float t) {
    return a + (b - a) * t;
  }

  /// @p values read between the four samples north-east of @p from.
  float between(const std::vector<float>& values, uint32_t from, uint32_t width,
                Vec2 at) {
    const float south = mixed(values[from], values[from + 1], at.x);
    const float north =
        mixed(values[from + width], values[from + width + 1], at.x);
    return mixed(south, north, at.y);
  }

  /// Carry the level, the speed and the foam one step downstream: each
  /// flowing sample takes what lay upstream of it a step ago. All three in
  /// one pass over where each comes from, read first and written after,
  /// so no sample reads a value already carried.
  void carry(WaterField& field) {
    const size_t count = field.carried.size();
    const uint32_t width = field.width;
    for (size_t k = 0; k < count; ++k) {
      const uint32_t from = field.carried_from[k];
      const Vec2 at{field.carried_x[k], field.carried_y[k]};
      field.scratch[3 * k] = between(field.level, from, width, at);
      field.scratch[3 * k + 1] = between(field.velocity, from, width, at);
      field.scratch[3 * k + 2] = between(field.foam, from, width, at);
    }
    for (size_t k = 0; k < count; ++k) {
      const uint32_t i = field.carried[k];
      field.level[i] = field.scratch[3 * k];
      field.velocity[i] = field.scratch[3 * k + 1];
      field.foam[i] = field.scratch[3 * k + 2];
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
  /// Spread each viscous sample's motion towards its neighbours': a thick
  /// fluid's viscosity, which stills a short ripple far sooner than a long
  /// swell. Scaled to the resolution, so the same fluid is as thick at
  /// every fidelity; the edge row is dry, as `accelerate` relies on.
  void thicken(WaterField& field) {
    const float per_tile = static_cast<float>(field.samples_per_tile) /
                           static_cast<float>(WATER_HIGH_SAMPLES_PER_TILE);
    const float spread = WATER_VISCOUS_SPREAD * per_tile * per_tile;
    std::vector<float>& v = field.velocity;
    for (uint32_t y = 1; y + 1 < field.height; ++y) {
      for (uint32_t x = 1; x + 1 < field.width; ++x) {
        const size_t i = slotOf(field, x, y);
        const float around = v[i - 1] + v[i + 1] + v[i - field.width] +
                             v[i + field.width] - 4.0f * v[i];
        v[i] += spread * field.viscosity[i] * around * field.keep[i];
      }
    }
  }

  /// Age the foam by one step: what there is thins, and a crest risen
  /// high enough to break throws more.
  void settleFoam(WaterField& field) {
    const float keep = std::exp(-WATER_STEP_SECONDS / WATER_FOAM_SECONDS);
    for (size_t i = 0; i < field.foam.size(); ++i) {
      const float breaking = std::max(0.0f, field.level[i] - WATER_FOAM_CREST) *
                             WATER_FOAM_PER_DEPTH;
      field.foam[i] = std::min(1.0f, field.foam[i] * keep + breaking);
    }
  }

  void stepOnce(WaterField& field) {
    accelerate(field);
    for (size_t i = 0; i < field.level.size(); ++i) {
      field.level[i] += field.velocity[i];
    }
    if (field.flowing) {
      carry(field);
    }
    if (field.viscous) {
      thicken(field);
    }
    settleFoam(field);
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
    const float sink = push.depth * bowl(distance, push.reach);
    field.level[i] -= sink;
    // Churned water foams: as much as the push was deep, past the depth a
    // drop of drizzle makes.
    field.foam[i] = std::min(
        1.0f, field.foam[i] + std::max(0.0f, sink - WATER_FOAM_CALM_DEPTH) *
                                  WATER_FOAM_PER_DEPTH);
    return true;
  }

  /// One step of `shoreThickness`: every dry sample takes the thickest of
  /// itself and its four neighbours.
  void spreadThicknessOnce(WaterField& field) {
    field.scratch = field.viscosity;
    const float* v = field.scratch.data();
    for (uint32_t y = 1; y + 1 < field.height; ++y) {
      for (uint32_t x = 1; x + 1 < field.width; ++x) {
        const size_t i = slotOf(field, x, y);
        if (field.keep[i] <= 0.0f) {
          field.viscosity[i] =
              std::max({v[i], v[i - 1], v[i + 1], v[i - field.width],
                        v[i + field.width]});
        }
      }
    }
  }

  /// Give each dry sample within `WATER_WET_TILES` of the water the
  /// thickness of the water beside it, so the shore it wets knows what
  /// laps at it. Nothing steps a dry sample, so the simulation never
  /// reads it.
  void shoreThickness(WaterField& field) {
    if (!field.viscous) {
      return;
    }
    const auto reach = static_cast<int>(std::ceil(
        WATER_WET_TILES * static_cast<float>(field.samples_per_tile)));
    for (int pass = 0; pass < reach; ++pass) {
      spreadThicknessOnce(field);
    }
    field.scratch.assign(3 * field.carried.size(), 0.0f);
  }

  /// Shape @p field, already sized, over @p layer's water and round each
  /// of @p obstacles: which samples are wet, how far each is from the
  /// shore, how deep and how fast it runs, and what it keeps a step.
  void shapeTo(WaterField& field, const WaterLayer& layer,
               std::span<const WaterObstacle> obstacles) {
    markWet(field, layer);
    for (const WaterObstacle& obstacle : obstacles) {
      dryUnder(field, obstacle);
    }
    measureShore(field);
    settleDepth(field, layer);
    settleKeep(field);
    traceFlow(field);
    shoreThickness(field);
  }

}  // namespace

void resetWaterField(WaterField& field, const WaterLayer& layer,
                     uint32_t samples_per_tile,
                     std::span<const WaterObstacle> obstacles) {
  GroundRect tiles = waterLayerBounds(layer);
  // A tile of dry land all round, so every wet sample has four neighbours
  // inside the field and the step never has to ask where the edge is.
  tiles = {tiles.x - 1, tiles.y - 1, tiles.width + 2, tiles.height + 2};
  const uint32_t resolution =
      tiles.width > 2
          ? fittingResolution(tiles, samples_per_tile, budgetFor(layer))
          : 0;
  sizeField(field, resolution > 0 ? tiles : GroundRect{}, resolution);
  if (resolution == 0) {
    return;
  }
  shapeTo(field, layer, obstacles);
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
