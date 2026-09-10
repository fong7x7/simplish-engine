#pragma once

/// @file editor-level-status.h
/// @brief What a level operation did, or why it did nothing.
/// @par Threading Thread-safe (immutable value type).

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

namespace eng::editor {

/// The outcome of creating a level or opening one.
///
/// Failure is in the signature rather than in a log line, because both
/// callers need to act on it: the editor puts it in the status line, and
/// the agent API turns it into a refusal an agent can read. A level
/// operation that only logged would leave the second of those with nothing
/// to say.
/// @thread_safety Immutable value type.
enum class EditorLevelStatus : uint8_t {
  /// The level asked for is now the open one.
  OK,
  /// No project is open, so there is nowhere for a level to live.
  NO_PROJECT,
  /// The id is not one the format allows.
  INVALID_ID,
  /// A level of that id is already in the project.
  ALREADY_EXISTS,
  /// No level of that id is in the project.
  NOT_FOUND,
  /// The open level holds edits that are not in its file.
  UNSAVED_CHANGES,
  /// The new level's file could not be written.
  WRITE_FAILED,
  /// The level was opened, but its file could not be parsed.
  UNREADABLE,
};

/// Every status, in the enum's own order.
inline constexpr EditorLevelStatus EDITOR_LEVEL_STATUSES[] = {
    EditorLevelStatus::OK,           EditorLevelStatus::NO_PROJECT,
    EditorLevelStatus::INVALID_ID,   EditorLevelStatus::ALREADY_EXISTS,
    EditorLevelStatus::NOT_FOUND,    EditorLevelStatus::UNSAVED_CHANGES,
    EditorLevelStatus::WRITE_FAILED, EditorLevelStatus::UNREADABLE,
};

/// One line per status, written for whoever is told it, indexed by the
/// status's own value.
///
/// A table rather than a switch, for the reason `AGENT_MENU_COMMAND_NAMES`
/// is one: the assertion below is what catches a status added to the enum
/// without anything to say about it.
inline constexpr std::string_view EDITOR_LEVEL_STATUS_MESSAGES[] = {
    "Level opened",
    "No project is open",
    "A level id starts with a letter: lowercase, digits, underscores",
    "The project already has a level of that id",
    "The project has no level of that id",
    "The open level has unsaved changes; save it first",
    "Could not write the level file",
    "Could not read that level's file",
};

static_assert(std::size(EDITOR_LEVEL_STATUS_MESSAGES) ==
                  std::size(EDITOR_LEVEL_STATUSES),
              "every level status needs a line saying what it means");

/// What @p status means, in one line.
[[nodiscard]] constexpr std::string_view
editorLevelStatusMessage(EditorLevelStatus status) {
  return EDITOR_LEVEL_STATUS_MESSAGES[static_cast<size_t>(status)];
}

}  // namespace eng::editor
