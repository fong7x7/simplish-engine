#pragma once

/// @file editor-tool.h
/// @brief The active level-authoring tool.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <string_view>

namespace eng::editor {

/// Tools on the editor toolbar. Order is the toolbar's left-to-right order.
/// @thread_safety Immutable value type.
enum class EditorTool : uint8_t {
  /// Select and move existing placements.
  SELECT,
  /// Paint terrain tiles onto the grid.
  TILE_PAINT,
  /// Raise and lower cell height.
  HEIGHT,
  /// Place static props.
  PROP,
  /// Place spawn points, objectives, and interactables.
  ENTITY,
};

/// Every tool in toolbar order.
inline constexpr EditorTool EDITOR_TOOLS[] = {
    EditorTool::SELECT, EditorTool::TILE_PAINT, EditorTool::HEIGHT,
    EditorTool::PROP,   EditorTool::ENTITY,
};

/// Toolbar button label for a tool.
[[nodiscard]] constexpr std::string_view editorToolLabel(EditorTool tool) {
  switch (tool) {
    case EditorTool::SELECT:
      return "Select";
    case EditorTool::TILE_PAINT:
      return "Tile";
    case EditorTool::HEIGHT:
      return "Height";
    case EditorTool::PROP:
      return "Prop";
    case EditorTool::ENTITY:
      return "Entity";
  }
  return "?";
}

}  // namespace eng::editor
