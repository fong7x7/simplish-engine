#pragma once

/// @file editor-sound-row-kind.h
/// @brief What a row of the Sound screen is.
/// @par Threading Thread-safe (constants only).

#include <cstdint>

namespace eng::editor {

/// Which kind of setting a row of the Sound screen shows, which decides how
/// it is drawn and what the keys do on it.
enum class EditorSoundRowKind : uint8_t {
  /// A title over the rows below it; never highlighted.
  HEADING,
  /// A volume, 0 to 1, drawn as a bar; left and right turn it.
  VOLUME,
  /// The mute, on or off; left, right or Enter switch it.
  MUTE,
  /// One of the game's sounds and the file it plays; left and right step
  /// through the project's sound files, Enter plays it.
  SLOT,
};

}  // namespace eng::editor
