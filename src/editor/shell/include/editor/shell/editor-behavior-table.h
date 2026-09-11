#pragma once

/// @file editor-behavior-table.h
/// @brief The project's behaviors, read from their data table.
/// @par Threading Main-thread-only (touches the filesystem).

#include <filesystem>
#include <game/content/behavior-definition.h>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// The schema every row of the behaviors table declares.
inline constexpr std::string_view EDITOR_BEHAVIOR_ENTRY_SCHEMA =
    "simplish/behavior/1.0";

/// The behaviors table as the editor read it.
/// @thread_safety Main-thread-only.
struct EditorBehaviorTable {
  /// Every behavior the file defines that could be read, in file order.
  /// The built-in behaviors are not here: they are the game's, and a row
  /// here with the same id replaces one (`game::resolveBehavior`).
  std::vector<game::BehaviorDefinition> behaviors{};
  /// What was wrong with the file, one line each, for the log and the agent
  /// API: a row or a state skipped, an exit that leads nowhere, a word the
  /// format does not know, a number held to its range. Empty for a clean
  /// file and for a project with no table at all.
  std::vector<std::string> problems{};
};

/// Where a project keeps its behaviors:
/// `<root>/content/data/behaviors.data.json`.
[[nodiscard]] std::filesystem::path
editorBehaviorTablePath(const std::filesystem::path& root);

/// The behaviors in the data table @p text (project-format §8.2).
///
/// Forgiving, as the characters reader is, because the file is written by
/// hand — but never so forgiving that a behavior it keeps could index past
/// its own states: every row it returns is `game::behaviorIsWellFormed`.
///
/// - A row with no usable id, or one an earlier row took, is skipped.
/// - A state with no usable id, or one an earlier state of the row took, is
///   skipped; a row left with no states is skipped.
/// - An exit whose `when` is not a condition, or whose `to` names no state,
///   is skipped; an unknown `do` is `idle` and an unknown `face` is
///   `movement`.
/// - A number that is not one takes its default; one out of range is held
///   to it.
///
/// Each of those is said in `problems`. A file that is not a behaviors
/// table gives no behaviors and one problem.
[[nodiscard]] EditorBehaviorTable
parseEditorBehaviorTable(std::string_view text);

/// The behaviors table under @p root. No behaviors and no problems when the
/// project has no table, which is most projects.
[[nodiscard]] EditorBehaviorTable
loadEditorBehaviorTable(const std::filesystem::path& root);

}  // namespace eng::editor
