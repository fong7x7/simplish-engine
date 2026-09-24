#pragma once

/// @file editor-water.h
/// @brief The ripples on a level's painted water, and what makes them.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/shell/editor-water-state.h>
#include <engine/math/vec2.h>
#include <engine/render-water/water-fidelity.h>
#include <engine/render-water/water-field.h>
#include <engine/render-water/water-layer.h>
#include <game/combat/combat-cue.h>
#include <span>
#include <vector>

namespace eng::editor {

/// How deep a walker's step pushes the water, in tiles, per tile walked.
inline constexpr float EDITOR_WADE_DEPTH_PER_TILE = 0.25f;

/// The deepest one frame of wading pushes, however far it went.
inline constexpr float EDITOR_WADE_MAX_DEPTH = 0.06f;

/// How wide a walker's wake is pushed, in tiles.
inline constexpr float EDITOR_WADE_RADIUS = 0.35f;

/// How deep, and how wide, a shot landing in the water pushes it.
inline constexpr float EDITOR_SPLASH_DEPTH = 0.12f;
inline constexpr float EDITOR_SPLASH_RADIUS = 0.3f;

/// How deep a blast over the water pushes it; it is as wide as half the
/// blast, and never narrower than `EDITOR_BLAST_MIN_RADIUS`.
inline constexpr float EDITOR_BLAST_DEPTH = 0.3f;
inline constexpr float EDITOR_BLAST_MIN_RADIUS = 0.5f;

/// The ripples on a level's water: shaped to its water layer at the user's
/// fidelity, pushed by whoever wades through it and by shots and blasts
/// landing in it, and aged on the frame's clock. At `FLAT` the water is
/// shaped and nothing else: nothing pushes it and it does not age.
///
/// Presentation only. It reads a playtest's positions and cues after the
/// ticks that made them, and nothing reads it back into one.
/// @thread_safety Main-thread-only.
class EditorWater {
public:
  /// Shape the water to @p layer at @p fidelity, unless it already is.
  /// Returns true when it was reshaped, which is when the surface wants
  /// building again. Everything moving is stilled.
  bool reshape(const WaterLayer& layer, WaterFidelity fidelity);

  /// The water it was last shaped to.
  [[nodiscard]] const WaterLayer& layer() const { return layer_; }

  /// Push the water wherever one of @p waders — in the same order as last
  /// time — has moved since the last call, by how far it went. A list of
  /// another length only starts following them.
  void wade(std::span<const Vec2> waders);

  /// Push the water where each of @p cues landed in it: a shot's hit
  /// splashes, a blast heaves.
  void splash(std::span<const game::CombatCue> cues);

  /// Forget where the waders were, so the next `wade` only starts following
  /// them: a playtest starting or stopping is a jump, not a step.
  void forgetWaders();

  /// Age the ripples by @p seconds of the frame's clock.
  void advance(float seconds);

  /// The field the surface is drawn from.
  [[nodiscard]] const WaterField& field() const { return field_; }

  /// The fidelity it was last shaped at.
  [[nodiscard]] WaterFidelity fidelity() const { return fidelity_; }

  /// The seconds it has been aged by, which the wind waves move on.
  [[nodiscard]] float seconds() const { return seconds_; }

  /// Bumped every time it is reshaped.
  [[nodiscard]] uint64_t shapeCount() const { return shape_; }

  /// Everything but `drawn` into @p state; the renderer knows that.
  void publish(EditorWaterState& state) const;

private:
  /// Push at @p at, counting it when it landed on water.
  void push(Vec2 at, float radius, float depth);

  /// The water it was last shaped to.
  WaterLayer layer_{};
  /// The fidelity it was last shaped at.
  WaterFidelity fidelity_ = WaterFidelity::FLAT;
  /// Whether it has been shaped at all.
  bool shaped_ = false;
  /// Bumped every time it is reshaped.
  uint64_t shape_ = 0;
  /// The ripples.
  WaterField field_{};
  /// Where each wader was at the last `wade`.
  std::vector<Vec2> waders_{};
  /// Pushes that landed on water.
  uint64_t pushes_ = 0;
  /// Seconds aged, wrapped where the wind waves wrap.
  float seconds_ = 0.0f;
};

}  // namespace eng::editor
