#pragma once

/// @file editor-effect-choices.h
/// @brief What a particle emitter's Effect row offers.
/// @par Threading Immutable value type.

#include <cstddef>
#include <string>
#include <vector>

namespace eng::editor {

/// The presets an emitter can be started from, in the order the Effect row
/// steps through them, and which one it was started from.
/// @thread_safety Immutable value type.
struct EditorEffectChoices {
  /// What the row shows for each: the preset's name, and — for the one the
  /// emitter was started from, once its numbers have been changed — that
  /// name marked `(edited)`.
  std::vector<std::string> names{};
  /// The preset id each choice starts the emitter from, alongside `names`.
  std::vector<std::string> ids{};
  /// Which choice the emitter was started from.
  size_t current = 0;
};

}  // namespace eng::editor
