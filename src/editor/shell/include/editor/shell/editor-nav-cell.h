#pragma once

/// @file editor-nav-cell.h
/// @brief What one navigation cell is, as the editor shows it.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// One cell of the level's navigation grid, sorted by what it means to an
/// actor a player's width (Editor REQUIREMENTS §4.3).
/// @thread_safety Immutable value type.
enum class EditorNavCell : uint8_t {
  /// Floor an actor can stand on and reach a player start from.
  OPEN,
  /// Inside a prop's box.
  SOLID,
  /// Not inside a prop, but too near one for an actor to stand in.
  NARROW,
  /// Floor an actor could stand on, walled off from every player start —
  /// almost always a mistake.
  UNREACHABLE,
};

}  // namespace eng::editor
