#pragma once

/// @file editor-build-command.h
/// @brief One program a build runs, and its arguments.
/// @par Threading Thread-safe (immutable value type).

#include <string>
#include <vector>

namespace eng::editor {

/// A command line as its words, the program first: quoted for the shell
/// only when it is run, so nothing before that has to know how.
/// @thread_safety Immutable value type.
struct EditorBuildCommand {
  /// The program, then each argument.
  std::vector<std::string> words;
};

}  // namespace eng::editor
