#pragma once

/// @file editor-emitter.h
/// @brief One particle emitter placed in the level.
/// @par Threading Main-thread-only.

#include <editor/shell/iso-projection.h>
#include <engine/math/vec3.h>
#include <engine/render-fx/fx-burst.h>
#include <engine/render-fx/fx-flash.h>
#include <string>

namespace eng::editor {

/// A particle emitter: a point in the level that throws one burst of
/// particles every `interval` seconds, and lights the scene with a flash
/// each time — while the level is being edited as well as while it is
/// played, so what a burst looks like can be tried out where it will be
/// seen ([fx.md](../../../../../docs/engine/fx.md)).
///
/// It holds the burst itself rather than naming one: `effect` says which of
/// the built-in presets it was last started from, and every number of the
/// burst can be changed from there. Presentation only — the simulation never
/// sees an emitter. Saved with the level as an entity of definition
/// `entity:fx_emitter` ([project-format.md §4.1]).
/// @thread_safety Main-thread-only.
struct EditorEmitter {
  /// Stable identifier for this one emitter: `emitter_01`. Assigned when it
  /// is added and never reused — see `editor-entity-id.h`.
  std::string id{};
  /// Where its bursts start: the middle of the tile it was dropped on,
  /// somewhat above the floor.
  WorldPoint position{};
  /// The preset it was last started from, by id: `wall_sparks`. What the
  /// Effect row shows, marked edited once the burst has been changed.
  std::string effect{};
  /// Which way its bursts point; any length, and zero throws every way.
  /// Straight up unless turned.
  Vec3 direction{0.0f, 0.0f, 1.0f};
  /// Seconds between bursts.
  float interval = 1.0f;
  /// The burst it throws.
  FxBurst burst{};
  /// The flash each burst lights the scene with; an intensity of zero is
  /// none.
  FxFlash flash{};
};

}  // namespace eng::editor
