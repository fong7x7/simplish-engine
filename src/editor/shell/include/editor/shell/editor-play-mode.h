#pragma once

/// @file editor-play-mode.h
/// @brief Whether the editor is editing the level or playing it.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// What the editor is doing with the open level.
///
/// Two modes rather than a flag on every panel: while playing, nothing may
/// change the document — the game is running from a copy of it, and an
/// edit would describe a level the running game is not in — so every
/// gesture that edits asks this one question first.
/// @thread_safety Immutable value type.
enum class EditorPlayMode : uint8_t {
  /// Placing, selecting and changing what the level holds.
  EDITING,
  /// Running the level in the real simulation (Editor REQUIREMENTS §7).
  PLAYING,
};

}  // namespace eng::editor
