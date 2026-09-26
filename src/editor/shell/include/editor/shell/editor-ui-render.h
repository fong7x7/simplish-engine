#pragma once

/// @file editor-ui-render.h
/// @brief The last game screen rendered to an image for an agent.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/ui/ui-button-info.h>
#include <game/ui/ui-node-info.h>
#include <string>
#include <vector>

namespace eng::editor {

/// What `render_ui_screen` last made: which screen, where its picture is,
/// and where each button was laid out in it — or why it could not.
struct EditorUiRender {
  /// The screen rendered.
  std::string id{};
  /// The PNG written, under the project's `build/ui/`; empty on failure.
  std::string path{};
  /// Its width, in pixels.
  uint32_t width = 0;
  /// Its height, in pixels.
  uint32_t height = 0;
  /// Whether a font was found to draw its text with.
  bool text = false;
  /// Every button, where it was laid out in the picture.
  std::vector<game::UiButtonInfo> buttons{};
  /// Every node with an id, where it was laid out, and its flags.
  std::vector<game::UiNodeInfo> nodes{};
  /// Why it could not be rendered; empty when it was.
  std::string error{};
};

}  // namespace eng::editor
