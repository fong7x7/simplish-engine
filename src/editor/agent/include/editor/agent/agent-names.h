#pragma once

/// @file agent-names.h
/// @brief The words the agent API calls the editor's enums by.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
#include <editor/shell/editor-action-kind.h>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-level-unsaved.h>
#include <editor/shell/editor-light.h>
#include <editor/shell/editor-menu-command.h>
#include <editor/shell/editor-play-mode.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/editor-selection.h>
#include <editor/shell/editor-tool.h>
#include <iterator>
#include <optional>
#include <string_view>

namespace eng::editor {

/// Wire name of every property field, indexed by the field's own value.
///
/// A table rather than a switch for the reason `EDITOR_PROPERTY_TRAITS` is
/// one, and asserted against the same array, so a field added to the enum
/// without a name here fails the build rather than being silently
/// unreachable from the API.
inline constexpr std::string_view AGENT_PROPERTY_FIELD_NAMES[] = {
    "position_x", "position_y",  "position_z",  "rotation_x",  "rotation_y",
    "rotation_z", "direction_x", "direction_y", "direction_z", "color_r",
    "color_g",    "color_b",     "intensity",   "range",       "player",
    "collides",   "scale",
};

static_assert(std::size(AGENT_PROPERTY_FIELD_NAMES) ==
                  std::size(EDITOR_ALL_PROPERTY_FIELDS),
              "every property field needs a name the agent API can use");

/// Wire name of every menu command, in `EditorMenuCommand` order.
///
/// The separator is unnamed on purpose: it is a layout marker the menu
/// never dispatches, and giving it a name would let `run_command` ask for
/// something that does not exist.
inline constexpr std::string_view AGENT_MENU_COMMAND_NAMES[] = {
    "",
    "new_project",
    "open_project",
    "save",
    "save_as",
    "new_level",
    "close_project",
    "exit",
    "undo",
    "redo",
    "cut",
    "copy",
    "paste",
    "delete_selection",
    "settings",
    "reset_view",
    "zoom_in",
    "zoom_out",
    "toggle_grid",
    "set_view_dimetric",
    "set_view_isometric",
    "set_shading_smooth",
    "set_shading_cel",
    "about",
    "playtest",
};

static_assert(std::size(AGENT_MENU_COMMAND_NAMES) ==
                  std::size(EDITOR_MENU_COMMAND_INFO),
              "every menu command needs a name the agent API can run it by");

/// Wire name of every toolbar tool, in `EditorTool` order.
inline constexpr std::string_view AGENT_EDITOR_TOOL_NAMES[] = {
    "select", "tile", "height", "prop", "entity",
};

static_assert(std::size(AGENT_EDITOR_TOOL_NAMES) == std::size(EDITOR_TOOLS),
              "every toolbar tool needs a name the agent API can select it by");

/// Wire name of one property field.
[[nodiscard]] constexpr std::string_view
agentPropertyFieldName(EditorPropertyField field) {
  return AGENT_PROPERTY_FIELD_NAMES[static_cast<size_t>(field)];
}

/// Wire name of one menu command, empty for the separator.
[[nodiscard]] constexpr std::string_view
agentMenuCommandName(EditorMenuCommand command) {
  return AGENT_MENU_COMMAND_NAMES[static_cast<size_t>(command)];
}

/// Wire name of one toolbar tool.
[[nodiscard]] constexpr std::string_view agentEditorToolName(EditorTool tool) {
  return AGENT_EDITOR_TOOL_NAMES[static_cast<size_t>(tool)];
}

/// Wire name of one light's kind.
[[nodiscard]] constexpr std::string_view
agentLightKindName(EditorLightKind kind) {
  return kind == EditorLightKind::DIRECTIONAL ? "directional" : "point";
}

/// Wire name of what a selection names.
[[nodiscard]] constexpr std::string_view
agentSelectionKindName(EditorSelectionKind kind) {
  switch (kind) {
    case EditorSelectionKind::PLACEMENT:
      return "placement";
    case EditorSelectionKind::LIGHT:
      return "light";
    case EditorSelectionKind::PLAYER_START:
      return "player_start";
    case EditorSelectionKind::NONE:
      return "none";
  }
  return "none";
}

/// Wire name of every recorded edit, indexed by the kind's own value.
///
/// A table for the reason `AGENT_PROPERTY_FIELD_NAMES` is one: nine arms of
/// two lines say no more than nine rows, and the assertion below catches a
/// kind added to the enum without a name here.
inline constexpr std::string_view AGENT_ACTION_KIND_NAMES[] = {
    "place_asset",      "transform_placement",    "remove_placement",
    "add_light",        "transform_light",        "remove_light",
    "add_player_start", "transform_player_start", "remove_player_start",
};

static_assert(std::size(AGENT_ACTION_KIND_NAMES) ==
                  static_cast<size_t>(EditorActionKind::REMOVE_PLAYER_START) +
                      1,
              "every recorded edit needs a name the agent API reports it by");

/// Wire name of one recorded edit.
[[nodiscard]] constexpr std::string_view
agentActionKindName(EditorActionKind kind) {
  return AGENT_ACTION_KIND_NAMES[static_cast<size_t>(kind)];
}

/// Wire name of whether the level is being edited, played, or is waiting on
/// the character selector.
[[nodiscard]] constexpr std::string_view
agentPlayModeName(EditorPlayMode mode) {
  switch (mode) {
    case EditorPlayMode::PLAYING:
      return "playing";
    case EditorPlayMode::CHOOSING:
      return "choosing";
    case EditorPlayMode::EDITING:
      break;
  }
  return "editing";
}

/// Wire name of how far an asset's card picture has got.
[[nodiscard]] constexpr std::string_view
agentThumbnailStateName(EditorAssetThumbnailState state) {
  switch (state) {
    case EditorAssetThumbnailState::PENDING:
      return "pending";
    case EditorAssetThumbnailState::READY:
      return "ready";
    case EditorAssetThumbnailState::FAILED:
      return "failed";
  }
  return "pending";
}

/// Wire name of what a level switch does about unwritten edits.
[[nodiscard]] constexpr std::string_view
agentLevelUnsavedName(EditorLevelUnsaved unsaved) {
  return unsaved == EditorLevelUnsaved::REFUSE ? "refuse" : "discard";
}

/// The unsaved-edit policy called @p name, or nothing when none is.
[[nodiscard]] constexpr std::optional<EditorLevelUnsaved>
findAgentLevelUnsaved(std::string_view name) {
  if (name == agentLevelUnsavedName(EditorLevelUnsaved::REFUSE)) {
    return EditorLevelUnsaved::REFUSE;
  }
  if (name == agentLevelUnsavedName(EditorLevelUnsaved::DISCARD)) {
    return EditorLevelUnsaved::DISCARD;
  }
  return std::nullopt;
}

/// The property field called @p name, or nothing when none is.
[[nodiscard]] constexpr std::optional<EditorPropertyField>
findAgentPropertyField(std::string_view name) {
  for (EditorPropertyField field : EDITOR_ALL_PROPERTY_FIELDS) {
    if (agentPropertyFieldName(field) == name) {
      return field;
    }
  }
  return std::nullopt;
}

/// The menu command called @p name, or nothing when none is.
[[nodiscard]] constexpr std::optional<EditorMenuCommand>
findAgentMenuCommand(std::string_view name) {
  for (const EditorMenuCommandInfo& info : EDITOR_MENU_COMMAND_INFO) {
    if (!name.empty() && agentMenuCommandName(info.command) == name) {
      return info.command;
    }
  }
  return std::nullopt;
}

/// The toolbar tool called @p name, or nothing when none is.
[[nodiscard]] constexpr std::optional<EditorTool>
findAgentEditorTool(std::string_view name) {
  for (EditorTool tool : EDITOR_TOOLS) {
    if (agentEditorToolName(tool) == name) {
      return tool;
    }
  }
  return std::nullopt;
}

/// The light kind called @p name, or nothing when none is.
[[nodiscard]] constexpr std::optional<EditorLightKind>
findAgentLightKind(std::string_view name) {
  if (name == "directional") {
    return EditorLightKind::DIRECTIONAL;
  }
  if (name == "point") {
    return EditorLightKind::POINT;
  }
  return std::nullopt;
}

}  // namespace eng::editor
