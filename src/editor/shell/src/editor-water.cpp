#include <algorithm>
#include <cmath>
#include <editor/shell/editor-water.h>

namespace eng::editor {

namespace {

  /// The seconds the water's clock wraps at: a whole number of every wind
  /// wave's period would be seamless, and this is long enough that the
  /// seam is never waited for.
  constexpr float WATER_CLOCK_WRAP = 1024.0f;

  /// Less than this, in tiles, a wader has not moved.
  constexpr float STILL_TILES = 1e-3f;

}  // namespace

bool EditorWater::reshape(const WaterLayer& layer, WaterFidelity fidelity) {
  if (shaped_ && fidelity == fidelity_ && layer == layer_) {
    return false;
  }
  layer_ = layer;
  fidelity_ = fidelity;
  shaped_ = true;
  ++shape_;
  resetWaterField(field_, layer_, waterSamplesPerTile(fidelity));
  return true;
}

void EditorWater::wade(std::span<const Vec2> waders) {
  if (waders.size() == waders_.size()) {
    for (size_t i = 0; i < waders.size(); ++i) {
      const float moved = Vec2::distance(waders[i], waders_[i]);
      if (moved > STILL_TILES) {
        push(waders[i], EDITOR_WADE_RADIUS,
             std::min(moved * EDITOR_WADE_DEPTH_PER_TILE,
                      EDITOR_WADE_MAX_DEPTH));
      }
    }
  }
  waders_.assign(waders.begin(), waders.end());
}

void EditorWater::splash(std::span<const game::CombatCue> cues) {
  for (const game::CombatCue& cue : cues) {
    const Vec2 at{cue.at.x, cue.at.y};
    if (cue.kind == game::CombatCueKind::BLAST) {
      push(at, std::max(cue.radius * 0.5f, EDITOR_BLAST_MIN_RADIUS),
           EDITOR_BLAST_DEPTH);
    } else if (cue.kind != game::CombatCueKind::SHOT_FIRED) {
      push(at, EDITOR_SPLASH_RADIUS, EDITOR_SPLASH_DEPTH);
    }
  }
}

void EditorWater::forgetWaders() {
  waders_.clear();
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
}

}  // namespace eng::editor
