#pragma once

/// @file input-bindings-load.h
/// @brief A control scheme read from a file, and what was wrong with it.
/// @par Threading
/// A value type.

#include <engine/input/input-bindings.h>
#include <string>
#include <vector>

namespace eng::input {

/// What `parseInputBindings` read: the scheme, and a sentence for each
/// part of the file it had to skip. A file with problems still loads —
/// one mistyped button should not cost a player every other binding.
struct InputBindingsLoad {
  /// The scheme the file describes, with defaults where it is silent.
  InputBindings bindings;
  /// One line per entry skipped, for the log.
  std::vector<std::string> problems;
};

}  // namespace eng::input
