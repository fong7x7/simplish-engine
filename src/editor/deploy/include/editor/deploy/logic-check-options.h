#pragma once

/// @file logic-check-options.h
/// @brief What `simplish-logic-check` is asked to check.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <filesystem>

namespace eng::editor {

/// The command line of `simplish-logic-check`.
/// @thread_safety Immutable value type.
struct LogicCheckOptions {
  /// The game logic library a build just made.
  std::filesystem::path library{};
  /// Content to run it on, laid out as a deployed game's: the manifest,
  /// the baked level, the data tables.
  std::filesystem::path content{};
  /// Ticks to run it for, unless the run ends first.
  uint64_t ticks = 600;
};

}  // namespace eng::editor
