#pragma once

/// @file agent-names.h
/// @brief The words the agent API calls the editor's enums by.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
#include <editor/shell/editor-action-kind.h>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-light.h>
#include <editor/shell/editor-menu-command.h>
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
    "color_g",    "color_b",     "intensity",   "range",
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
    "close_project",
    "exit",
    "undo",
    "redo",
    "cut",
    "copy",
    "paste",
    "settings",
    "reset_view",
    "zoom_in",
    "zoom_out",
    "toggle_grid",
    "set_view_dimetric",
    "set_view_isometric",
    "about",
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
    case EditorSelectionKind::NONE:
      return "none";
  }
  return "none";
}

/// Wire name of one recorded edit.
[[nodiscard]] constexpr std::string_view
agentActionKindName(EditorActionKind kind) {
  switch (kind) {
    case EditorActionKind::PLACE_ASSET:
      return "place_asset";
    case EditorActionKind::TRANSFORM_PLACEMENT:
      return "transform_placement";
    case EditorActionKind::ADD_LIGHT:
      return "add_light";
    case EditorActionKind::TRANSFORM_LIGHT:
      return "transform_light";
  }
  return "place_asset";
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
