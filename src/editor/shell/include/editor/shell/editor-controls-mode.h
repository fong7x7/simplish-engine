#pragma once

/// @file editor-controls-mode.h
/// @brief Whether the Controls screen is choosing a row or listening.
/// @par Threading Thread-safe (constants only).

#include <cstdint>

namespace eng::editor {

/// What the Controls screen is doing with the next key or button.
enum class EditorControlsMode : uint8_t {
  /// Moving between actions: arrows or the d-pad choose one.
  BROWSING,
  /// Listening: the next key or pad control is bound to the chosen action.
  LISTENING,
};

}  // namespace eng::editor
