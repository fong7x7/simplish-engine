#include <algorithm>
#include <cmath>
#include <editor/shell/editor-water.h>
#include <utility>

namespace eng::editor {

namespace {

  /// The seconds the water's clock wraps at: a whole number of every wind
  /// wave's period would be seamless, and this is long enough that the
  /// seam is never waited for.
  constexpr float WATER_CLOCK_WRAP = 1024.0f;

  /// Less than this, in tiles, a wader has not moved.
  constexpr float STILL_TILES = 1e-3f;

}  // namespace

bool EditorWater::reshape(const WaterLayer& layer, WaterFidelity fidelity,
                          std::span<const WaterObstacle> obstacles) {
  if (shaped_ && fidelity == fidelity_ && layer == layer_ &&
      std::ranges::equal(obstacles, obstacles_)) {
    return false;
  }
  layer_ = layer;
  fidelity_ = fidelity;
  obstacles_.assign(obstacles.begin(), obstacles.end());
  shaped_ = true;
  ++shape_;
  resetWaterField(field_, layer_, waterSamplesPerTile(fidelity), obstacles_);
  return true;
}

void EditorWater::wade(std::span<const Vec2> waders) {
  if (waders.size() == waders_.size()) {
    for (size_t i = 0; i < waders.size(); ++i) {
      follow(i, waders_[i], waders[i]);
    }
  } else {
    strides_.assign(waders.size(), 0.0f);
  }
  waders_.assign(waders.begin(), waders.end());
}

void EditorWater::follow(size_t i, Vec2 from, Vec2 to) {
  const float moved = Vec2::distance(to, from);
  if (moved <= STILL_TILES) {
    return;
  }
  const float depth =
      std::min(moved * EDITOR_WADE_DEPTH_PER_TILE, EDITOR_WADE_MAX_DEPTH);
  push(to, EDITOR_WADE_RADIUS, depth);
  wake(to, (to - from) * (1.0f / moved), depth);
  strides_[i] += waterFieldWetAt(field_, to) ? moved : 0.0f;
  if (strides_[i] >= EDITOR_WADE_SPLASH_TILES) {
    strides_[i] = 0.0f;
    owe(to, EDITOR_WADE_SPLASH_SCALE);
  }
}

void EditorWater::wake(Vec2 at, Vec2 heading, float depth) {
  // Pushed afresh every frame along two arms trailing behind, so the V
  // they draw follows the wader and the ripples spread from it.
  const Vec2 side{-heading.y, heading.x};
  for (int step = 1; step <= 3; ++step) {
    const float back = EDITOR_WAKE_TILES * static_cast<float>(step) / 3.0f;
    const Vec2 middle = at - heading * back;
    const Vec2 spread = side * (back * EDITOR_WAKE_SPREAD);
    const float arm = depth * 0.3f * (1.0f - back / (EDITOR_WAKE_TILES * 1.4f));
    push(middle + spread, EDITOR_WADE_RADIUS * 0.5f, arm);
    push(middle - spread, EDITOR_WADE_RADIUS * 0.5f, arm);
  }
}

void EditorWater::splash(std::span<const game::CombatCue> cues) {
  for (const game::CombatCue& cue : cues) {
    const Vec2 at{cue.at.x, cue.at.y};
    if (cue.kind == game::CombatCueKind::BLAST) {
      push(at, std::max(cue.radius * 0.5f, EDITOR_BLAST_MIN_RADIUS),
           EDITOR_BLAST_DEPTH);
      owe(at, EDITOR_BLAST_SPLASH_SCALE);
    } else if (cue.kind != game::CombatCueKind::SHOT_FIRED) {
      push(at, EDITOR_SPLASH_RADIUS, EDITOR_SPLASH_DEPTH);
      owe(at, EDITOR_SHOT_SPLASH_SCALE);
    }
  }
}

void EditorWater::forgetWaders() {
  waders_.clear();
  strides_.clear();
}

void EditorWater::owe(Vec2 at, float scale) {
  if (waterFieldWetAt(field_, at)) {
    splashes_.push_back({at, scale});
    ++splashed_;
  }
}

std::vector<EditorWaterSplash> EditorWater::takeSplashes() {
  return std::exchange(splashes_, {});
}

void EditorWater::advance(float seconds) {
  if (!(seconds > 0.0f)) {
    return;
  }
  // A still surface does not move; its clock still runs, and does nothing.
  if (waterFidelitySimulates(fidelity_)) {
    stepWaterField(field_, seconds);
  }
  seconds_ = std::fmod(seconds_ + seconds, WATER_CLOCK_WRAP);
}

void EditorWater::push(Vec2 at, float radius, float depth) {
  // Nothing would carry a push away, so a still surface takes none.
  if (waterFidelitySimulates(fidelity_) &&
      disturbWaterField(field_, at, radius, depth)) {
    ++pushes_;
  }
}

void EditorWater::publish(EditorWaterState& state) const {
  state.fidelity = fidelity_;
  state.samples_per_tile = field_.samples_per_tile;
  state.wet_samples = field_.wet_count;
  state.energy = waterFieldEnergy(field_);
  state.pushes = pushes_;
  state.splashes = splashed_;
  state.obstacles = static_cast<uint32_t>(obstacles_.size());
}

}  // namespace eng::editor
