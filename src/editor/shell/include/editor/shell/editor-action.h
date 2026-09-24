#pragma once

/// @file editor-action.h
/// @brief One recorded, reversible editor operation.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <editor/shell/editor-action-kind.h>
#include <editor/shell/editor-emitter.h>
#include <editor/shell/editor-ground-change.h>
#include <editor/shell/editor-light.h>
#include <editor/shell/editor-placement.h>
#include <editor/shell/editor-player-start.h>
#include <editor/shell/editor-sprite.h>
#include <editor/shell/editor-water-change.h>
#include <editor/shell/editor-waypoint.h>
#include <vector>

namespace eng::editor {

/// A mutating operation, holding everything needed to undo and redo it.
///
/// Actions are values, not command objects with virtual `apply` and
/// `revert`. There is one document and a small set of operations on it, so
/// a tagged record keeps the history copyable and comparable, keeps every
/// operation's inverse readable in one file, and costs no allocation per
/// edit — where a class hierarchy would add one heap node per action and
/// spread the inverses across as many files as there are tools.
/// @thread_safety Main-thread-only.
struct EditorAction {
  /// Which operation this record describes.
  EditorActionKind kind = EditorActionKind::PLACE_ASSET;
  /// Which entry of the list its kind names — placements, lights or
  /// player starts — the operation added, removed, or changed.
  size_t index = 0;
  /// The placement the operation names: what was added, what a transform
  /// changed it to, or what a removal took out. Kept so redo restores it
  /// exactly — and so undoing a removal can put the entry back rather than
  /// an empty one wearing its index.
  EditorPlacement placement{};
  /// The placement as it was before a transform, and unused by every other
  /// kind. An edit that replaces a value cannot be inverted from the value
  /// alone, so the record carries both halves rather than making undo
  /// reconstruct one.
  EditorPlacement prior{};
  /// The light the operation names, for the three kinds that name one.
  /// Carried in the same record as the placement rather than in a variant:
  /// a light is six numbers, and a record that is always the same shape
  /// stays copyable, comparable, and free of a heap node per edit.
  EditorLight light{};
  /// The light as it was before a transform, unused by every other kind.
  EditorLight light_prior{};
  /// The player start the operation names, for the three kinds that name
  /// one — carried beside the others for the reason `light` is.
  EditorPlayerStart player_start{};
  /// The start as it was before a transform, unused by every other kind.
  EditorPlayerStart player_start_prior{};
  /// The waypoint the operation names, for the three kinds that name one.
  EditorWaypoint waypoint{};
  /// The waypoint as it was before a transform, unused by every other kind.
  EditorWaypoint waypoint_prior{};
  /// The particle emitter the operation names, for the three kinds that
  /// name one.
  EditorEmitter emitter{};
  /// The emitter as it was before a transform, unused by every other kind.
  EditorEmitter emitter_prior{};
  /// The sprite billboard the operation names, for the three kinds that
  /// name one.
  EditorSprite sprite{};
  /// The billboard as it was before a transform, unused by every other
  /// kind.
  EditorSprite sprite_prior{};
  /// Every cell a ground paint changed, with what it held on both sides,
  /// and empty for every other kind. The one field here that allocates: a
  /// stroke repaints however many cells it crossed, and a list of them is
  /// both the smallest record of that and its own inverse.
  std::vector<EditorGroundChange> ground{};
  /// Every cell whose water the paint changed — laid, dried, deepened or
  /// recoloured — with its water on both sides. A stroke carries its
  /// terrain and its water together; an edit to the water alone leaves
  /// `ground` empty.
  std::vector<EditorWaterChange> water{};
};

}  // namespace eng::editor
