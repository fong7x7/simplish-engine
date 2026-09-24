#pragma once

/// @file editor-water-change.h
/// @brief One cell a water edit changed.
/// @par Threading Thread-safe (immutable value type).

#include <engine/render-ground/ground-cell.h>
#include <engine/render-water/water-cell.h>

namespace eng::editor {

/// A cell of the water layer an edit changed, with the water on both sides
/// of the change — a depth of 0 on a side is dry. An edit to the water is
/// the list of these, and undoing it is writing each `before` back.
/// @thread_safety Immutable value type.
struct EditorWaterChange {
  /// Which cell.
  GroundCell cell{};
  /// The water it held before.
  WaterCell before{};
  /// The water it holds after.
  WaterCell after{};

  /// Two changes are the same when they change the same cell the same way.
  bool operator==(const EditorWaterChange&) const = default;
};

}  // namespace eng::editor
